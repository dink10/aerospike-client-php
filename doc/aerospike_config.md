
# Runtime Configuration

The following configuration options in php.ini

| Name  | Default  |
|:------|:---------:|
| aerospike.connect_timeout | 1000 |
| aerospike.read_timeout | 1000 |
| aerospike.write_timeout | 1000 |
| aerospike.key_policy | digest |
| aerospike.serializer | php |
| aerospike.udf.lua_system_path | /opt/aerospike/lua |
| aerospike.udf.lua_user_path | /opt/aerospike/usr-lua |
| aerospike.shm.use | false |
| aerospike.shm.key | 0xA5000000 |
| aerospike.shm.max_nodes | 16 |
| aerospike.shm.max_namespaces | 8 |
| aerospike.shm.takeover_threshold_sec | 30 |
| aerospike.use_batch_direct | 0 |
| aerospike.compression_threshold | 0 |
| aerospike.max_threads | 300 |
| aerospike.thread_pool_size | 16 |

Here is a description of the configuration directives:

**aerospike.connect_timeout integer**
    The connection timeout in milliseconds

**aerospike.read_timeout integer**
    The read timeout in milliseconds

**aerospike.write_timeout integer**
    The write timeout in milliseconds

**aerospike.key_policy string**
    Whether to send and store the record's (ns,set,key) data along with its (unique identifier) digest. One of { digest, send }

**aerospike.serializer string**
    The unsupported type handler. One of { php, user, none }

**aerospike.udf.lua_system_path string**
    Path to the system support files for Lua UDFs

**aerospike.udf.lua_user_path string**
    Path to the user-defined Lua function modules

**aerospike.shm.use boolean**
    Indicates if shared memory should be used for cluster tending. Recommended for multi-process cases such as FPM. One of { true, false }

**aerospike.shm.key string**
    Explicitly sets the shm key for this client to store shared-memory cluster tending results in.

**aerospike.shm.max_nodes integer**
    Shared memory maximum number of server nodes allowed. Leave a cushion so new nodes can be added without needing a client restart.

**aerospike.shm.max_namespaces integer**
    Shared memory maximum number of namespaces allowed. Leave a cushion for new namespaces.

**aerospike.shm.takeover_threshold_sec integer**
    Take over shared memory cluster tending if the cluster hasn't been tended by this threshold in seconds.

**aerospike.use_batch_direct**
    Use the batch-direct (1) or batch-index (0) protocol for batch read operations.

**aerospike.compression_threshold**
    The client will compress records larger than this value for transport.

**aerospike.max_threads integer**
    Size of the synchronous connection pool for each server node

**aerospike.thread_pool_size integer**
    Number of threads stored in underlying thread pool that is used in batch/scan/query commands

**aerospike.compression_threshold**
    Minimum record size beyond which it is compressed and sent to the server

## See Also

### [Aerospike Class](aerospike.md)
### [Aerospike Session Handler](aerospike_sessions.md)
