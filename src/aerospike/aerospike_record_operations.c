/*
 *
 * Copyright (C) 2014-2016 Aerospike, Inc.
 *
 * Portions may be licensed to Aerospike, Inc. under one or more contributor
 * license agreements.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

#include "php.h"
#include "ext/standard/php_var.h"
#include "ext/standard/php_smart_str.h"
#include "aerospike/as_status.h"
#include "aerospike/aerospike_key.h"
#include "aerospike/as_error.h"
#include "aerospike/as_record.h"
#include "aerospike/as_boolean.h"
#include "string.h"
#include "aerospike_common.h"
#include "aerospike_policy.h"
#include "aerospike_general_constants.h"
#include "aerospike_transform.h"

extern bool operater_ordered_callback(const char *key, const as_val *value, void *array TSRMLS_DC)
{
    zval* record_local_p;
    static int iterator = 0;
    as_error err;

    MAKE_STD_ZVAL(record_local_p);
    array_init(record_local_p);
    if (key) {
        if (0 != add_next_index_string(record_local_p, key, 1)) {
            DEBUG_PHP_EXT_DEBUG("Unable to get the record key.");
            return false;
        }
    }

    if (value) {
        switch(value->type) {
            case AS_STRING:
                if (0 != add_next_index_string(record_local_p, as_string_get((as_string *) value), 1)) {
                    DEBUG_PHP_EXT_DEBUG("Unable to get the record.");
                    return false;
                }
                break;

            case AS_INTEGER:
                if (0 != add_next_index_long(record_local_p, (long) as_integer_get((as_integer *) value))) {
                    DEBUG_PHP_EXT_DEBUG("Unable to get the record.");
                    return false;
                }
                break;

            case AS_BOOLEAN:
                if (0 != add_next_index_bool(record_local_p, (int) as_boolean_get((as_boolean *)value))) {
                    DEBUG_PHP_EXT_DEBUG("Unable to get the record.");
                    return false;
                }

            case AS_BYTES:
                ADD_LIST_APPEND_BYTES(NULL, NULL, (void *)value, &record_local_p, &err TSRMLS_CC);
                if (err.code != AEROSPIKE_OK) {
                    DEBUG_PHP_EXT_DEBUG("Unable to get the record.");
                    return false;
                }
                break;

            case AS_UNDEF:
            case AS_NIL:
                ADD_LIST_APPEND_NULL(NULL, NULL, (void *)value, &record_local_p, &err TSRMLS_CC);
                if (err.code != AEROSPIKE_OK) {
                    DEBUG_PHP_EXT_DEBUG("Unable to get the record.");
                    return false;
                }
                break;

            case AS_DOUBLE:
                 ADD_LIST_APPEND_DOUBLE(NULL, NULL, (void *)value, &record_local_p, &err TSRMLS_CC);
                 if (err.code != AEROSPIKE_OK) {
                     DEBUG_PHP_EXT_DEBUG("Unable to get the record.");
                     return false;
                 }
                 break;

            case AS_LIST:
                 ADD_LIST_APPEND_LIST(NULL, (void *)key, (void *)value, &record_local_p, &err TSRMLS_CC);
                 if (err.code != AEROSPIKE_OK) {
                     DEBUG_PHP_EXT_DEBUG("Unable to get the record.");
                     return false;
                 }
                 break;

            case AS_MAP:
                 ADD_LIST_APPEND_MAP(NULL, NULL, (void *)value, &record_local_p, &err TSRMLS_CC);
                 if (err.code != AEROSPIKE_OK) {
                     DEBUG_PHP_EXT_DEBUG("Unable to get the record.");
                     return false;
                 }
                 break;

            case AS_REC:
                 ADD_LIST_APPEND_REC(NULL, NULL, (void *)value, &record_local_p, &err TSRMLS_CC);
                 if (err.code != AEROSPIKE_OK) {
                     DEBUG_PHP_EXT_DEBUG("Unable to get the record.");
                     return false;
                 }
                 break;

            case AS_PAIR:
                 ADD_LIST_APPEND_PAIR(NULL, NULL, (void *)value, &record_local_p, &err TSRMLS_CC);
                 if (err.code != AEROSPIKE_OK) {
                     DEBUG_PHP_EXT_DEBUG("Unable to get the record.");
                     return false;
                 }
                 break;
        }
    } else {
        if (0 != add_next_index_null(record_local_p)) {
            DEBUG_PHP_EXT_DEBUG("Unable to get the record.");
            return false;
        }
    }
    if(0 != add_index_zval(((foreach_callback_udata *) array)->udata_p, iterator, record_local_p)) {
        DEBUG_PHP_EXT_DEBUG("Unable to get the record.");
        return false;
    }
    iterator++;

    return true;
}

/*
 *******************************************************************************************************
 * Wrapper function to perform an aerospike_key_oeprate within the C client.
 *
 * @param as_object_p           The C client's aerospike object.
 * @param as_key_p              The C client's as_key that identifies the record.
 * @param options_p             The user's optional policy options to be used if set, else defaults.
 * @param error_p               The as_error to be populated by the function
 *                              with the encountered error if any.
 * @param bin_name_p            The bin name to perform operation upon.
 * @param str                   The string to be appended in case of operation: append.
 * @param offset                The offset to be incremented by in case of operation: increment,
 *                              or to be initialized if record/bin does not already exist.
 * @param time_to_live          The ttl for the record in case of operation: touch.
 * @param operation             The operation type.
 *
 *******************************************************************************************************
 */
static as_status
aerospike_record_operations_ops(Aerospike_object *aerospike_obj_p,
								aerospike* as_object_p,
                                as_key* as_key_p,
                                zval* options_p,
                                as_error* error_p,
                                char* bin_name_p,
                                char* str,
                                char* geoStr,
                                u_int64_t offset,
                                u_int32_t time_to_live,
                                u_int64_t operation,
                                as_operations* ops,
                                zval** each_operation,
                                as_policy_operate* operate_policy,
                                int8_t serializer_policy,
                                as_record** get_rec TSRMLS_DC)
{
    as_val*                value_p = NULL;
    const char             *select[] = {bin_name_p, NULL};
    zval*                  temp_record_p = NULL;
    zval*                  append_val_copy = NULL;
    as_record              record;
    as_static_pool         static_pool = {0};
    as_val                 *val = NULL;
    as_arraylist           args_list;
    as_arraylist*          args_list_p = NULL;
    as_static_pool         items_pool = {0};

    as_error_init(error_p);
    as_record_inita(&record, 1);


    switch (operation) {
        case AS_OPERATOR_APPEND:
            if (!as_operations_add_append_str(ops, bin_name_p, str)) {
                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to append");
                DEBUG_PHP_EXT_DEBUG("Unable to append");
                goto exit;
            }
            break;
        case AS_OPERATOR_PREPEND:
            if (!as_operations_add_prepend_str(ops, bin_name_p, str)) {
                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to prepend");
                DEBUG_PHP_EXT_DEBUG("Unable to prepend");
                goto exit;
            }
            break;
        case AS_OPERATOR_INCR:
            if (!as_operations_add_incr(ops, bin_name_p, offset)) {
                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to increment");
                DEBUG_PHP_EXT_DEBUG("Unable to increment");
                goto exit;
            }
            break;
        case AS_OPERATOR_TOUCH:
            ops->ttl = time_to_live;
            if (!as_operations_add_touch(ops)) {
                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to touch");
                DEBUG_PHP_EXT_DEBUG("Unable to touch");
                goto exit;
            }
            break;
        case AS_OPERATOR_READ:
            if (!as_operations_add_read(ops, bin_name_p)) {
                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to read");
                DEBUG_PHP_EXT_DEBUG("Unable to read");
                goto exit;
            }
            break;

        case AS_OPERATOR_WRITE:
            if (str) {
                if (!as_operations_add_write_str(ops, bin_name_p, str)) {
                    PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to write");
                    DEBUG_PHP_EXT_DEBUG("Unable to write");
                    goto exit;
                }
            } else if (geoStr) {
                if (!as_operations_add_write_geojson_str(ops, bin_name_p, geoStr)) {
                    PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to write");
                    DEBUG_PHP_EXT_DEBUG("Unable to write");
                    goto exit;
                }
            }else {
                if (!as_operations_add_write_int64(ops, bin_name_p, offset)) {
                    PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to write");
                    DEBUG_PHP_EXT_DEBUG("Unable to write");
                    goto exit;
                }
            }
            break;

        case AS_CDT_OP_LIST_APPEND_NEW:
            MAKE_STD_ZVAL(temp_record_p);
            array_init(temp_record_p);

            ALLOC_ZVAL(append_val_copy);
            MAKE_COPY_ZVAL(each_operation, append_val_copy);
            add_assoc_zval(temp_record_p, bin_name_p, append_val_copy);

            aerospike_transform_iterate_records(aerospike_obj_p, &temp_record_p, &record, &static_pool, serializer_policy, aerospike_has_double(as_object_p), error_p TSRMLS_CC);
            if (AEROSPIKE_OK != error_p->code) {
                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_PARAM, "Unable to parse the value parameter");
                DEBUG_PHP_EXT_ERROR("Unable to parse the value parameter");
                 if (temp_record_p) {
                     zval_ptr_dtor(&temp_record_p);
                 }
                 if (append_val_copy) {
                     zval_ptr_dtor(&append_val_copy);
                 }
                 goto exit;
            }
            val = (as_val*) as_record_get(&record, bin_name_p);
            if (val) {
                if (!as_operations_add_list_append(ops, bin_name_p, val)) {
                    PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to append");
                    DEBUG_PHP_EXT_DEBUG("Unable to append");
                    if (temp_record_p) {
                        zval_ptr_dtor(&temp_record_p);
                    }
                    if (append_val_copy) {
                        zval_ptr_dtor(&append_val_copy);
                    }
                    goto exit;
                }
            }
            if (temp_record_p) {
                zval_ptr_dtor(&temp_record_p);
            }
            if (append_val_copy) {
                zval_ptr_dtor(&append_val_copy);
            }
            break;

        case AS_CDT_OP_LIST_INSERT_NEW:
            MAKE_STD_ZVAL(temp_record_p);
            array_init(temp_record_p);

            ALLOC_ZVAL(append_val_copy);
            MAKE_COPY_ZVAL(each_operation, append_val_copy);
            add_assoc_zval(temp_record_p, bin_name_p, append_val_copy);

            aerospike_transform_iterate_records(aerospike_obj_p, &temp_record_p, &record, &static_pool, serializer_policy, aerospike_has_double(as_object_p), error_p TSRMLS_CC);
            if (AEROSPIKE_OK != error_p->code) {
                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_PARAM, "Unable to parse the value parameter");
                DEBUG_PHP_EXT_ERROR("Unable to parse the value parameter");
                if (temp_record_p) {
                    zval_ptr_dtor(&temp_record_p);
                }
                if (append_val_copy) {
                    zval_ptr_dtor(&append_val_copy);
                }
                goto exit;
            }
            val = (as_val*) as_record_get(&record, bin_name_p);
            if (val) {
                if (!as_operations_add_list_insert(ops, bin_name_p, time_to_live, val)) {
                    PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to insert");
                    DEBUG_PHP_EXT_DEBUG("Unable to insert");
                    if (temp_record_p) {
                        zval_ptr_dtor(&temp_record_p);
                    }
                    if (append_val_copy) {
                        zval_ptr_dtor(&append_val_copy);
                    }
                    goto exit;
                }
            }
            if (temp_record_p) {
                zval_ptr_dtor(&temp_record_p);
            }
            if (append_val_copy) {
                zval_ptr_dtor(&append_val_copy);
            }
            break;

        case AS_CDT_OP_LIST_INSERT_ITEMS_NEW:
            if (Z_TYPE_PP(each_operation) != IS_ARRAY) {
                DEBUG_PHP_EXT_DEBUG("Value passed if not array type.");
                goto exit;
            }
            as_arraylist_inita(&args_list, zend_hash_num_elements(Z_ARRVAL_PP(each_operation)));
            args_list_p = &args_list;

            AS_LIST_PUT(aerospike_obj_p, NULL, each_operation, args_list_p, &items_pool, serializer_policy,
                    error_p TSRMLS_CC);

            if (error_p->code == AEROSPIKE_OK) {
                if (!as_operations_add_list_append_items(ops, bin_name_p, (as_list*)args_list_p)) {
                    PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to insert items.");
                    DEBUG_PHP_EXT_DEBUG("Unable to insert items.");
                    goto exit;
                }
            }
            break;

        case AS_CDT_OP_LIST_POP_NEW:
            if (!as_operations_add_list_pop(ops, bin_name_p, time_to_live)) {
                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to pop.");
                DEBUG_PHP_EXT_DEBUG("Unable to pop.");
                goto exit;
            }
            break;

        case AS_CDT_OP_LIST_POP_RANGE_NEW:
            if (!as_operations_add_list_pop_range(ops, bin_name_p, time_to_live, offset)) {
                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to pop range.");
                DEBUG_PHP_EXT_DEBUG("Unable to pop range.");
                goto exit;
            }
            break;

        case AS_CDT_OP_LIST_REMOVE_NEW:
            if (!as_operations_add_list_remove(ops, bin_name_p, time_to_live)) {
                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to remove.");
                DEBUG_PHP_EXT_DEBUG("Unable to remove.");
                goto exit;
            }
            break;

        case AS_CDT_OP_LIST_REMOVE_RANGE_NEW:
            if (!as_operations_add_list_remove_range(ops, bin_name_p, time_to_live, offset)) {
                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to remove range.");
                DEBUG_PHP_EXT_DEBUG("Unable to remove range.");
                goto exit;
            }
            break;

        case AS_CDT_OP_LIST_CLEAR_NEW:
            if (!as_operations_add_list_clear(ops, bin_name_p)) {
                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to clear.");
                DEBUG_PHP_EXT_DEBUG("Unable to clear.");
                goto exit;
            }
            break;

        case AS_CDT_OP_LIST_SET_NEW:
            MAKE_STD_ZVAL(temp_record_p);
            array_init(temp_record_p);

            ALLOC_ZVAL(append_val_copy);
            MAKE_COPY_ZVAL(each_operation, append_val_copy);
            add_assoc_zval(temp_record_p, bin_name_p, append_val_copy);

            aerospike_transform_iterate_records(aerospike_obj_p, &temp_record_p, &record, &static_pool, serializer_policy, aerospike_has_double(as_object_p), error_p TSRMLS_CC);
            if (AEROSPIKE_OK != error_p->code) {
                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_PARAM, "Unable to parse the value parameter");
                DEBUG_PHP_EXT_ERROR("Unable to parse the value parameter");
                goto exit;
            }
            val = (as_val*) as_record_get(&record, bin_name_p);
            if (val) {
                if (!as_operations_add_list_set(ops, bin_name_p, time_to_live, val)){
                    PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to set.");
                    DEBUG_PHP_EXT_DEBUG("Unable to set.");
                    goto exit;
                }
            }
            break;

        case AS_CDT_OP_LIST_GET_NEW:
            if (!as_operations_add_list_get(ops, bin_name_p, time_to_live)) {
                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to get.");
                DEBUG_PHP_EXT_DEBUG("Unable to get.");
                goto exit;
            }
            break;

        case AS_CDT_OP_LIST_GET_RANGE_NEW:
            if (!as_operations_add_list_get_range(ops, bin_name_p, time_to_live, offset)) {
                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to get range.");
                DEBUG_PHP_EXT_DEBUG("Unable to  get range.");
                goto exit;
            }
            break;

        case AS_CDT_OP_LIST_TRIM_NEW:
            if (!as_operations_add_list_trim(ops, bin_name_p, time_to_live, offset)) {
                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to trim.");
                DEBUG_PHP_EXT_DEBUG("Unable to trim.");
                goto exit;
            }
            break;

        case AS_CDT_OP_LIST_SIZE_NEW:
            if (!as_operations_add_list_size(ops, bin_name_p)) {
                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to get size.");
                DEBUG_PHP_EXT_DEBUG("Unable to get size.");
                goto exit;
            }
            break;

        default:
            PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Invalid operation");
            DEBUG_PHP_EXT_DEBUG("Invalid operation");
            goto exit;
    }

exit:
    if (args_list_p) {
        as_arraylist_destroy(args_list_p);
    }
    return error_p->code;
}

/*
 *******************************************************************************************************
 * Wrapper function to perform an aerospike_key_exists within the C client.
 *
 * @param as_object_p           The C client's aerospike object.
 * @param as_key_p              The C client's as_key that identifies the record.
 * @param error_p               The as_error to be populated by the function
 *                              with the encountered error if any.
 * @param metadata_p            The return metadata for the exists/getMetadata API to be 
 *                              populated by this function.
 * @param options_p             The user's optional policy options to be used if set, else defaults.
 *
 *******************************************************************************************************
 */
extern as_status aerospike_record_operations_exists(aerospike* as_object_p,
        as_key* as_key_p,
        as_error *error_p,
        zval* metadata_p,
        zval* options_p TSRMLS_DC)
{
    as_policy_read              read_policy;
    as_record*                  record_p = NULL;

    if ((!as_key_p) || (!error_p) || (!as_object_p) || (!metadata_p)) {
        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Cannot perform exists");
        DEBUG_PHP_EXT_DEBUG("Cannot perform exists");
        goto exit;
    }

    set_policy(&as_object_p->config, &read_policy, NULL, NULL, NULL, NULL,
            NULL, NULL, NULL, options_p, error_p TSRMLS_CC);
    if (AEROSPIKE_OK != error_p->code) {
        DEBUG_PHP_EXT_DEBUG("Unable to set policy");
        goto exit;
    }

    if (AEROSPIKE_OK != aerospike_key_exists(as_object_p, error_p,
                &read_policy, as_key_p, &record_p)) {
        goto exit;
    }

    add_assoc_long(metadata_p, "generation", record_p->gen);
    add_assoc_long(metadata_p, "ttl", record_p->ttl);

exit:
    if (record_p) {
        as_record_destroy(record_p);
    }

    return error_p->code;
}


/*
 *******************************************************************************************************
 * Wrapper function to perform an aerospike_key_remove within the C client.
 *
 * @param as_object_p           The C client's aerospike object.
 * @param as_key_p              The C client's as_key that identifies the record.
 * @param error_p               The as_error to be populated by the function
 *                              with the encountered error if any.
 * @param options_p             The user's optional policy options to be used if set, else defaults.
 *
 *******************************************************************************************************
 */
    extern as_status
aerospike_record_operations_remove(Aerospike_object* aerospike_obj_p,
        as_key* as_key_p,
        as_error *error_p,
        zval* options_p)
{
    as_policy_remove            remove_policy;
    aerospike*                  as_object_p = aerospike_obj_p->as_ref_p->as_p;
    TSRMLS_FETCH_FROM_CTX(aerospike_obj_p->ts);

    if ( (!as_key_p) || (!error_p) || (!as_object_p)) {
        DEBUG_PHP_EXT_DEBUG("Unable to remove key");
        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to remove key");
        goto exit;
    }

    set_policy(&as_object_p->config, NULL, NULL, NULL, &remove_policy,
            NULL, NULL, NULL, NULL, options_p, error_p TSRMLS_CC);
    if (AEROSPIKE_OK != error_p->code) {
        DEBUG_PHP_EXT_DEBUG("Unable to set policy");
        goto exit;
    }

    get_generation_value(options_p, &remove_policy.generation, error_p TSRMLS_CC);
    if (error_p->code == AEROSPIKE_OK) {
        aerospike_key_remove(as_object_p, error_p, &remove_policy, as_key_p);
    }

exit: 
    return error_p->code;
}

    static as_status
aerospike_record_initialization(aerospike* as_object_p,
        as_key* as_key_p,
        zval* options_p,
        as_error* error_p,
        as_policy_operate* operate_policy,
        int8_t* serializer_policy TSRMLS_DC)
{
    as_policy_operate_init(operate_policy);

    if ((!as_object_p) || (!error_p) || (!as_key_p)) {
        DEBUG_PHP_EXT_DEBUG("Unable to perform operate");
        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to perform operate");
        goto exit;
    }

    set_policy(&as_object_p->config, NULL, NULL, operate_policy, NULL, NULL,
            NULL, NULL, serializer_policy, options_p, error_p TSRMLS_CC);
    if (AEROSPIKE_OK != error_p->code) {
        DEBUG_PHP_EXT_DEBUG("Unable to set policy");
        goto exit;
    }
exit:
    return error_p->code;
}

    extern as_status
aerospike_record_operations_general(Aerospike_object* aerospike_obj_p,
        as_key* as_key_p,
        zval* options_p,
        as_error* error_p,
        char* bin_name_p,
        char* str,
        u_int64_t offset,
        u_int64_t time_to_live,
        u_int64_t operation)
{
    as_operations       ops;
    as_record*          get_rec = NULL;
    aerospike*          as_object_p = aerospike_obj_p->as_ref_p->as_p;
    as_policy_operate   operate_policy;
    /*
     * TODO: serializer_policy is not used right now.
     * Need to pass on serializer_policy to aerospike_record_operations_ops
     * in case of write operation, to write bytes to the database.
     */
    int8_t            serializer_policy;

    TSRMLS_FETCH_FROM_CTX(aerospike_obj_p->ts);
    as_operations_inita(&ops, 1);
    get_generation_value(options_p, &ops.gen, error_p TSRMLS_CC);
    if (error_p->code != AEROSPIKE_OK) {
        goto exit;
    }
    if (AEROSPIKE_OK != aerospike_record_initialization(as_object_p, as_key_p,
                options_p, error_p,
                &operate_policy,
                &serializer_policy TSRMLS_CC)) {
        DEBUG_PHP_EXT_ERROR("Initialization returned error");
        goto exit;
    }

    if (AEROSPIKE_OK != aerospike_record_operations_ops(aerospike_obj_p, as_object_p, as_key_p,
                options_p, error_p,
                bin_name_p, str, NULL,
                offset, time_to_live, operation,
                &ops, NULL, NULL, 0, &get_rec TSRMLS_CC)) {
        DEBUG_PHP_EXT_ERROR("Prepend function returned an error");
        goto exit;
    }

    if (AEROSPIKE_OK == get_options_ttl_value(options_p, &ops.ttl, error_p TSRMLS_CC)) {
        aerospike_key_operate(as_object_p, error_p, &operate_policy,
                as_key_p, &ops, NULL);
    }

exit: 
    if (get_rec) {
        as_record_destroy(get_rec);
    }
    as_operations_destroy(&ops);
    return error_p->code;
}

    extern as_status
aerospike_record_operations_operate(Aerospike_object* aerospike_obj_p,
        as_key* as_key_p,
        zval* options_p,
        as_error* error_p,
        zval* returned_p,
        HashTable* operations_array_p)
{
    as_operations               ops;
    as_record*                  get_rec = NULL;
    aerospike*                  as_object_p = aerospike_obj_p->as_ref_p->as_p;
    as_status                   status = AEROSPIKE_OK;
    as_policy_operate           operate_policy;
    HashPosition                pointer;
    HashPosition                each_pointer;
    HashTable*                  each_operation_array_p = NULL;
    char*                       bin_name_p;
    char*                       str;
    char*                       geoStr;
    zval **                     operation;
    int                         offset = 0;
    long                        l_offset = 0;
    int                         op;
    zval**                      each_operation;
    zval**                      each_operation_back;
    int8_t                      serializer_policy;
    uint32_t                    ttl;                /* Using this same variable for 'index' as well 
                                                     * in case of OP_LIST_INSERT. 
                                                     * This will avoid adding another parameter to
                                                     * further function and can reuse 'ttl'. 
                                                     */
    foreach_callback_udata      foreach_record_callback_udata;

    TSRMLS_FETCH_FROM_CTX(aerospike_obj_p->ts);
    as_operations_inita(&ops, zend_hash_num_elements(operations_array_p));

    get_generation_value(options_p, &ops.gen, error_p TSRMLS_CC);
    if (error_p->code != AEROSPIKE_OK) {
        goto exit;
    }

    if (AEROSPIKE_OK !=
            (status = aerospike_record_initialization(as_object_p, as_key_p,
                                                      options_p, error_p,
                                                      &operate_policy,
                                                      &serializer_policy TSRMLS_CC))) {
        DEBUG_PHP_EXT_ERROR("Initialization returned error");
        goto exit;
    }

    foreach_hashtable(operations_array_p, pointer, operation) {
        as_record *temp_rec = NULL;

        if (IS_ARRAY == Z_TYPE_PP(operation)) {
            each_operation_array_p = Z_ARRVAL_PP(operation);
            str = NULL;
            geoStr = NULL;
            op = 0;
            ttl = 0;
            bin_name_p = NULL;
            foreach_hashtable(each_operation_array_p, each_pointer, each_operation) {
                uint options_key_len;
                ulong options_index;
                char* options_key;
                if (zend_hash_get_current_key_ex(each_operation_array_p, (char **) &options_key, 
                            &options_key_len, &options_index, 0, &each_pointer)
                        != HASH_KEY_IS_STRING) {
                    DEBUG_PHP_EXT_DEBUG("Unable to set policy: Invalid Policy Constant Key");
                    PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
                            "Unable to set policy: Invalid Policy Constant Key");
                    goto exit;
                } else {
                    if (!strcmp(options_key, "op") && (IS_LONG == Z_TYPE_PP(each_operation))) {
                        op = (uint32_t) Z_LVAL_PP(each_operation);
                    } else if (!strcmp(options_key, "bin") && (IS_STRING == Z_TYPE_PP(each_operation))) {
                        bin_name_p = (char *) Z_STRVAL_PP(each_operation);
                    } else if (!strcmp(options_key, "val")) {
                        if (IS_STRING == Z_TYPE_PP(each_operation)) {
                            str = (char *) Z_STRVAL_PP(each_operation);
                            each_operation_back = each_operation;
                        } else if (IS_LONG == Z_TYPE_PP(each_operation)) {
                            offset = (uint32_t) Z_LVAL_PP(each_operation);
                        } else if (IS_OBJECT == Z_TYPE_PP(each_operation)) {
                            const char* name;
                            zend_uint name_len;
                            int dup;
                            dup = zend_get_object_classname(*((zval**)each_operation),
                                    &name, &name_len TSRMLS_CC);
                            if((!strcmp(name, GEOJSONCLASS)) 
                                    && (aerospike_obj_p->hasGeoJSON)
                                    && op == AS_OPERATOR_WRITE) {
                                int result;
                                zval* retval = NULL, *fname = NULL;
                                ALLOC_INIT_ZVAL(fname);
                                ZVAL_STRINGL(fname, "__tostring", sizeof("__tostring") -1, 1);
                                result = call_user_function_ex(NULL, each_operation, fname, &retval,
                                        0, NULL, 0, NULL TSRMLS_CC);

                                geoStr = (char *) malloc (strlen(Z_STRVAL_P(retval)) + 1);
                                if (geoStr == NULL) {
                                    DEBUG_PHP_EXT_DEBUG("Failed to allocate memory\n");
                                    PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
                                            "Failed to allocate memory\n");
                                    goto exit;
                                }
                                memset(geoStr, '\0', strlen(geoStr));
                                strcpy(geoStr, Z_STRVAL_P(retval));

                                if (name) {
                                    efree((void *) name);
                                    name = NULL;
                                }
                                if (fname) {
                                    zval_ptr_dtor(&fname);
                                }
                                if (retval) {
                                    zval_ptr_dtor(&retval);
                                }
                            }
                            else {
                                status = AEROSPIKE_ERR_CLIENT;
                                DEBUG_PHP_EXT_DEBUG("Invalid operation on GeoJSON datatype OR Old version of server, "
                                        "GeoJSON not supported on this server");
                                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Invalid operation on GeoJSON "
                                        "datatype OR Old version of server, GeoJSON not supported on this server");
                                goto exit;
                            }
                        } else if (IS_ARRAY == Z_TYPE_PP(each_operation)) {
                        } else {
                            status = AEROSPIKE_ERR_CLIENT;
                            goto exit;
                        }
                    } else if (!strcmp(options_key, "ttl") && (IS_LONG == Z_TYPE_PP(each_operation))) {
                        ttl = (uint32_t) Z_LVAL_PP(each_operation);
                    } else if (!strcmp(options_key, "index") && (IS_LONG == Z_TYPE_PP(each_operation))) {
                        ttl = (uint32_t) Z_LVAL_PP(each_operation);
                    } else {
                        status = AEROSPIKE_ERR_CLIENT;
                        DEBUG_PHP_EXT_DEBUG("Unable to set Operate: Invalid Optiopns Key");
                        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to set Operate: Invalid Options Key");
                        goto exit;
                    }
                }
            }
            if (op == AS_OPERATOR_INCR) {
                if (str) {
                    l_offset = (long) offset;
                    if  (!(is_numeric_string(Z_STRVAL_PP(each_operation_back), Z_STRLEN_PP(each_operation_back), &l_offset, NULL, 0))) {
                        status = AEROSPIKE_ERR_PARAM;
                        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_PARAM, "invalid value for increment operation");
                        DEBUG_PHP_EXT_DEBUG("Invalid value for increment operation");
                        goto exit;
                    }
                }
            }
            if (AEROSPIKE_OK != (status = aerospike_record_operations_ops(aerospike_obj_p, as_object_p,
                            as_key_p, options_p, error_p, bin_name_p, str, geoStr,
                            offset, ttl, op, &ops, each_operation, &operate_policy,
                            serializer_policy, &temp_rec TSRMLS_CC))) {
                DEBUG_PHP_EXT_ERROR("Operate function returned an error");
                goto exit;
            }
            if (temp_rec) {
                as_record_destroy(temp_rec);
            }
        } else {
            status = AEROSPIKE_ERR_CLIENT;
            goto exit;
        }
    }

    if (AEROSPIKE_OK != get_options_ttl_value(options_p, &ops.ttl, error_p TSRMLS_CC)) {
        goto exit;
    }

    if (AEROSPIKE_OK != (status = aerospike_key_operate(as_object_p, error_p,
                    &operate_policy, as_key_p, &ops, &get_rec))) {
        DEBUG_PHP_EXT_DEBUG("%s", error_p->message);
        goto exit;
    } else {
        if (get_rec) {
            foreach_record_callback_udata.udata_p = returned_p;
            foreach_record_callback_udata.error_p = error_p;
            foreach_record_callback_udata.obj = aerospike_obj_p;
            if (!as_record_foreach(get_rec, (as_rec_foreach_callback) AS_DEFAULT_GET,
                        &foreach_record_callback_udata)) {
                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
                        "Unable to get bins of a record");
                DEBUG_PHP_EXT_DEBUG("Unable to get bins of a record");
            }
        }
    }

exit: 
    if (get_rec) {
        foreach_record_callback_udata.udata_p = NULL;
        as_record_destroy(get_rec);
    }
    as_operations_destroy(&ops);
    return status;
}

    extern as_status
aerospike_record_operations_operate_ordered(Aerospike_object* aerospike_obj_p,
        as_key* as_key_p,
        zval* options_p,
        as_error* error_p,
        zval* returned_p,
        HashTable* operations_array_p)
{
    as_operations               ops;
    as_record*                  get_rec = NULL;
    as_record*                  get_rec_temp = NULL;
    aerospike*                  as_object_p = aerospike_obj_p->as_ref_p->as_p;
    as_status                   status = AEROSPIKE_OK;
    as_policy_operate           operate_policy;
    HashPosition                pointer;
    HashPosition                each_pointer;
    HashTable*                  each_operation_array_p = NULL;
    char*                       bin_name_p;
    char*                       str;
    char*                       geoStr;
    zval **                     operation;
    int                         offset = 0;
    long                        l_offset = 0;
    int                         op;
    zval**                      each_operation;
    zval**                      each_operation_back;
    int8_t                      serializer_policy;
    uint32_t                    ttl;
    foreach_callback_udata      foreach_record_callback_udata;
    zval*                       record_p_local;
    zval*                       metadata_container_p;
    zval*                       key_container_p;

    MAKE_STD_ZVAL(record_p_local)
        array_init(record_p_local);

    MAKE_STD_ZVAL(metadata_container_p);
    array_init(metadata_container_p);

    MAKE_STD_ZVAL(key_container_p);
    array_init(key_container_p);

    TSRMLS_FETCH_FROM_CTX(aerospike_obj_p->ts);

    if (AEROSPIKE_OK !=
            (status = aerospike_record_initialization(as_object_p, as_key_p,
                                                      options_p, error_p,
                                                      &operate_policy,
                                                      &serializer_policy TSRMLS_CC))) {
        DEBUG_PHP_EXT_ERROR("Initialization returned error");
        goto exit;
    }

    foreach_hashtable(operations_array_p, pointer, operation) {
        as_operations_inita(&ops, zend_hash_num_elements(operations_array_p));

        get_generation_value(options_p, &ops.gen, error_p TSRMLS_CC);
        if (error_p->code != AEROSPIKE_OK) {
            goto exit;
        }
        as_record *temp_rec = NULL;

        if (IS_ARRAY == Z_TYPE_PP(operation)) {
            each_operation_array_p = Z_ARRVAL_PP(operation);
            str = NULL;
            geoStr = NULL;
            op = 0;
            ttl = 0;
            bin_name_p = NULL;
            foreach_hashtable(each_operation_array_p, each_pointer, each_operation) {
                uint options_key_len;
                ulong options_index;
                char* options_key;
                if (zend_hash_get_current_key_ex(each_operation_array_p, (char **) &options_key, 
                            &options_key_len, &options_index, 0, &each_pointer)
                        != HASH_KEY_IS_STRING) {
                    DEBUG_PHP_EXT_DEBUG("Unable to set policy: Invalid Policy Constant Key");
                    PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
                            "Unable to set policy: Invalid Policy Constant Key");
                    goto exit;
                } else {
                    if (!strcmp(options_key, "op") && (IS_LONG == Z_TYPE_PP(each_operation))) {
                        op = (uint32_t) Z_LVAL_PP(each_operation);
                    } else if (!strcmp(options_key, "bin") && (IS_STRING == Z_TYPE_PP(each_operation))) {
                        bin_name_p = (char *) Z_STRVAL_PP(each_operation);
                    } else if (!strcmp(options_key, "val")) {
                        if (IS_STRING == Z_TYPE_PP(each_operation)) {
                            str = (char *) Z_STRVAL_PP(each_operation);
                            each_operation_back = each_operation;
                        } else if (IS_LONG == Z_TYPE_PP(each_operation)) {
                            offset = (uint32_t) Z_LVAL_PP(each_operation);
                        } else if (IS_OBJECT == Z_TYPE_PP(each_operation)) {
                            const char* name;
                            zend_uint name_len;
                            int dup;
                            dup = zend_get_object_classname(*((zval**)each_operation),
                                    &name, &name_len TSRMLS_CC);
                            if((!strcmp(name, GEOJSONCLASS))
                                    && (aerospike_obj_p->hasGeoJSON)
                                    && op == AS_OPERATOR_WRITE) {
                                int result;
                                zval* retval = NULL, fname;
                                ZVAL_STRINGL(&fname, "__tostring", sizeof("__tostring") -1, 1);
                                result = call_user_function_ex(NULL, each_operation, &fname, &retval,
                                        0, NULL, 0, NULL TSRMLS_CC);
                                geoStr = Z_STRVAL_P(retval);
                            }
                            else {
                                status = AEROSPIKE_ERR_CLIENT;
                                DEBUG_PHP_EXT_DEBUG("Invalid operation on GeoJSON datatype OR Old version of server, "
                                        "GeoJSON not supported on this server");
                                PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Invalid operation on GeoJSON "
                                        "datatype OR Old version of server, GeoJSON not supported on this server");
                                goto exit;
                            }
                        } else if (IS_ARRAY == Z_TYPE_PP(each_operation)) {
                        } else {
                            status = AEROSPIKE_ERR_CLIENT;
                            goto exit;
                        }
                    } else if (!strcmp(options_key, "ttl") && (IS_LONG == Z_TYPE_PP(each_operation))) {
                        ttl = (uint32_t) Z_LVAL_PP(each_operation);
                    } else if (!strcmp(options_key, "index") && (IS_LONG == Z_TYPE_PP(each_operation))) {
                        ttl = (uint32_t) Z_LVAL_PP(each_operation);
                    } else {
                        status = AEROSPIKE_ERR_CLIENT;
                        DEBUG_PHP_EXT_DEBUG("Unable to set Operate: Invalid Optiopns Key");
                        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to set Operate: Invalid Options Key");
                        goto exit;
                    }
                }
            }
            if (op == AS_OPERATOR_INCR) {
                if (str) {
                    l_offset = (long) offset;
                    if  (!(is_numeric_string(Z_STRVAL_PP(each_operation_back), Z_STRLEN_PP(each_operation_back), &l_offset, NULL, 0))) {
                        status = AEROSPIKE_ERR_PARAM;
                        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_PARAM, "invalid value for increment operation");
                        DEBUG_PHP_EXT_DEBUG("Invalid value for increment operation");
                        goto exit;
                    }
                }
            }
            if (AEROSPIKE_OK != (status = aerospike_record_operations_ops(aerospike_obj_p, as_object_p,
                            as_key_p, options_p, error_p, bin_name_p, str, geoStr,
                            offset, ttl, op, &ops, each_operation, &operate_policy,
                            serializer_policy, &temp_rec TSRMLS_CC))) {
                DEBUG_PHP_EXT_ERROR("Operate function returned an error");
                goto exit;
            }
            if (temp_rec) {
                as_record_destroy(temp_rec);
            }
        } else {
            status = AEROSPIKE_ERR_CLIENT;
            goto exit;
        }

        if (AEROSPIKE_OK != get_options_ttl_value(options_p, &ops.ttl, error_p TSRMLS_CC)) {
            goto exit;
        }
        foreach_record_callback_udata.udata_p = record_p_local;
        foreach_record_callback_udata.error_p = error_p;
        foreach_record_callback_udata.obj = aerospike_obj_p;

        if (AEROSPIKE_OK != (status = aerospike_key_operate(as_object_p, error_p,
                        &operate_policy, as_key_p, &ops, &get_rec))) {
            DEBUG_PHP_EXT_DEBUG("%s", error_p->message);
            operater_ordered_callback(bin_name_p, NULL, &foreach_record_callback_udata TSRMLS_CC);
            goto exit;
        } else {
            if (get_rec) {
                if (!((op == AS_OPERATOR_READ ) ||
                            (op == AS_CDT_OP_LIST_SIZE_NEW) || 
                            (op == AS_CDT_OP_LIST_GET_NEW)   ||
                            (op == AS_CDT_OP_LIST_GET_RANGE_NEW) ||
                            (op == AS_CDT_OP_LIST_POP_NEW)   ||
                            (op == AS_CDT_OP_LIST_POP_RANGE_NEW))) {
                    if (!operater_ordered_callback(bin_name_p, NULL, &foreach_record_callback_udata TSRMLS_CC)) {
                        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
                                "Unable to get bins of a record");
                        DEBUG_PHP_EXT_DEBUG("Unable to get bins of a record");
                    }
                } else if (get_rec->bins.size == 0){
                    if (!operater_ordered_callback(bin_name_p, NULL, &foreach_record_callback_udata TSRMLS_CC)) {
                        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
                                "Unable to get bins of a record");
                        DEBUG_PHP_EXT_DEBUG("Unable to get bins of a record");
                    }
                }else {
                    if (!as_record_foreach(get_rec, (as_rec_foreach_callback) operater_ordered_callback, 
                                &foreach_record_callback_udata)) {
                        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT,
                                "Unable to get bins of a record");
                        DEBUG_PHP_EXT_DEBUG("Unable to get bins of a record");
                    }
                }
            }
        }
        as_operations_destroy(&ops);
        if (get_rec) {
            if (get_rec_temp) {
                as_record_destroy(get_rec_temp);
                get_rec_temp = NULL;
            }
            get_rec_temp = get_rec;
            get_rec = NULL;
        }
    }

exit:
    status = aerospike_get_record_metadata(get_rec_temp, metadata_container_p TSRMLS_CC);
    if (status != AEROSPIKE_OK) {
        DEBUG_PHP_EXT_DEBUG("Unable to get metadata of record.");
        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_PARAM, "Unable to get metadata of record.");
        goto exit_1;
    }


    status = aerospike_get_record_key_digest(&(aerospike_obj_p->as_ref_p->as_p->config), get_rec_temp, as_key_p, key_container_p, options_p, true TSRMLS_CC);
    if (status != AEROSPIKE_OK) {
        DEBUG_PHP_EXT_DEBUG("Unable to get key and digest for record.");
        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_PARAM, "Unable to get key and digest for record.");
        goto exit_1;
    }

    if (0 != add_assoc_zval(returned_p, PHP_AS_KEY_DEFINE_FOR_KEY, key_container_p)) {
        DEBUG_PHP_EXT_DEBUG("Unable to get key of a record.");
        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_PARAM, "Unable to get key of a record.");
        goto exit_1;
    }

    if (0 != add_assoc_zval(returned_p, PHP_AS_RECORD_DEFINE_FOR_METADATA, metadata_container_p)) {
        DEBUG_PHP_EXT_DEBUG("Unable to get metadata of a record.");
        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_PARAM, "Unable to get metadata of a record.");
        goto exit_1;
    }

    if (0 != add_assoc_zval(returned_p, PHP_AS_RECORD_DEFINE_FOR_RESULTS, record_p_local)) {
        DEBUG_PHP_EXT_DEBUG("Unable to get the record.");
        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_PARAM, "Unable to get the record.");
        goto exit_1;
    }

exit_1:
    if (get_rec_temp) {
        foreach_record_callback_udata.udata_p = NULL;
        as_record_destroy(get_rec_temp);
        get_rec_temp = NULL;
    }
    return status;
}

/*
 *******************************************************************************************************
 * Wrapper function to remove bin(s) from a record.
 *
 * @param as_object_p           The C client's aerospike object.
 * @param as_key_p              The C client's as_key that identifies the record.
 * @param bins_p                The PHP array of bins to be removed from the record.
 * @param error_p               The as_error to be populated by the function
 *                              with the encountered error if any.
 * @param options_p             The user's optional policy options to be used if set, else defaults.
 *
 *******************************************************************************************************
 */
    extern as_status
aerospike_record_operations_remove_bin(Aerospike_object* aerospike_obj_p,
        as_key* as_key_p,
        zval* bins_p,
        as_error* error_p,
        zval* options_p)
{
    as_status           status = AEROSPIKE_OK;
    as_record           rec;
    HashTable           *bins_array_p = Z_ARRVAL_P(bins_p);
    HashPosition        pointer;
    zval                **bin_names;
    as_policy_write     write_policy;
    aerospike*          as_object_p = aerospike_obj_p->as_ref_p->as_p;
    TSRMLS_FETCH_FROM_CTX(aerospike_obj_p->ts);

    as_record_inita(&rec, zend_hash_num_elements(bins_array_p));

    if ((!as_object_p) || (!error_p) || (!as_key_p) || (!bins_array_p)) {
        status = AEROSPIKE_ERR_CLIENT;
        goto exit;
    }

    set_policy(&as_object_p->config, NULL, &write_policy, NULL, NULL, NULL,
            NULL, NULL, NULL, options_p, error_p TSRMLS_CC);
    if (AEROSPIKE_OK != (status = (error_p->code))) {
        DEBUG_PHP_EXT_DEBUG("Unable to set policy");
        goto exit;
    }


    foreach_hashtable(bins_array_p, pointer, bin_names) {
        if (IS_STRING == Z_TYPE_PP(bin_names)) {
            if (!(as_record_set_nil(&rec, Z_STRVAL_PP(bin_names)))) {
                status = AEROSPIKE_ERR_CLIENT;
                goto exit;
            }
        } else {
            status = AEROSPIKE_ERR_CLIENT;
            goto exit;
        }
    }

    get_generation_value(options_p, &rec.gen, error_p TSRMLS_CC);
    if (error_p->code == AEROSPIKE_OK) {
        if (AEROSPIKE_OK == get_options_ttl_value(options_p, &rec.ttl,
                    error_p TSRMLS_CC)) {
            if (AEROSPIKE_OK != (status = aerospike_key_put(as_object_p, error_p,
                            NULL, as_key_p, &rec))) {
                goto exit;
            }
        }
    }

exit:
    as_record_destroy(&rec);
    return(status);
}

/*
 *******************************************************************************************************
 * Wrapper function to perform an aerospike_key_exists within the C client.
 *
 * @param as_object_p           The C client's aerospike object.
 * @param as_key_p              The C client's as_key that identifies the record.
 * @param metadata_p            The return metadata for the exists/getMetadata API to be 
 *                              populated by this function.
 * @param options_p             The user's optional policy options to be used if set, else defaults.
 * @param error_p               The as_error to be populated by the function
 *                              with the encountered error if any.
 *
 *******************************************************************************************************
 */
    extern as_status
aerospike_php_exists_metadata(Aerospike_object* aerospike_obj_p,
        zval* key_record_p,
        zval* metadata_p,
        zval* options_p,
        as_error* error_p)
{
    as_status              status = AEROSPIKE_OK;
    as_key                 as_key_for_put_record;
    int16_t                initializeKey = 0;
    aerospike*             as_object_p = aerospike_obj_p->as_ref_p->as_p;

    TSRMLS_FETCH_FROM_CTX(aerospike_obj_p->ts);
    if ((!as_object_p) || (!key_record_p)) {
        DEBUG_PHP_EXT_DEBUG("Unable to perform exists");
        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_CLIENT, "Unable to perform exists");
        status = AEROSPIKE_ERR_CLIENT;
        goto exit;
    }

    if (PHP_TYPE_ISNOTARR(key_record_p) ||
            ((options_p) && (PHP_TYPE_ISNOTARR(options_p)))) {
        PHP_EXT_SET_AS_ERR(error_p, AEROSPIKE_ERR_PARAM,
                "input parameters (type) for exist/getMetdata function not proper.");
        DEBUG_PHP_EXT_ERROR("input parameters (type) for exist/getMetdata function not proper.");
        status = AEROSPIKE_ERR_PARAM;
        goto exit;
    }

    if (PHP_TYPE_ISNOTARR(metadata_p)) {
        zval*         metadata_arr_p = NULL;

        MAKE_STD_ZVAL(metadata_arr_p);
        array_init(metadata_arr_p);
        ZVAL_ZVAL(metadata_p, metadata_arr_p, 1, 1);
    }

    if (AEROSPIKE_OK != (status =
                aerospike_transform_iterate_for_rec_key_params(Z_ARRVAL_P(key_record_p),
                    &as_key_for_put_record, &initializeKey))) {
        PHP_EXT_SET_AS_ERR(error_p, status,
                "unable to iterate through exists/getMetadata key params");
        DEBUG_PHP_EXT_ERROR("unable to iterate through exists/getMetadata key params");
        goto exit;
    }

    if (AEROSPIKE_OK != (status =
                aerospike_record_operations_exists(as_object_p, &as_key_for_put_record,
                    error_p, metadata_p, options_p TSRMLS_CC))) {
        DEBUG_PHP_EXT_ERROR("exists/getMetadata: unable to fetch the record");
        goto exit;
    }

exit:
    if (initializeKey) {
        as_key_destroy(&as_key_for_put_record);
    }

    return status;
}
