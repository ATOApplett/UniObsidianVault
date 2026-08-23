Test create_shared_object: all system calls succeed. (0/4)
Setup:
    shared_memory_t shm;
    bool ok = create_shared_object( &shm, "/some_random_file_name");

Function returned false, which is not correct.
Test failed!

Test create_shared_object: shm_open_fails. (0/1)
Setup:
    shared_memory_t shm;
    bool ok = create_shared_object( &shm, "/some_random_file_name");

Function call sequence does not match the expected value.

Observed function call sequence:
BEGIN OBSERVED
shm_unlink("lWSwqXO6euXjXVu") succeeds
shm_open("lWSwqXO6euXjXVu", 66, 0666) fails
ftruncate(-1, 96) succeeds

END OBSERVED

Expected function call sequence:
BEGIN EXPECTED
shm_unlink("lWSwqXO6euXjXVu") succeeds
shm_open("lWSwqXO6euXjXVu", 66, 0666) fails

END EXPECTED

Test failed!

Test create_shared_object: shm_open succeed, but ftruncate fails. (0/1)
Setup:
    shared_memory_t shm;
    bool ok = create_shared_object( &shm, "/some_random_file_name");

Function returned true, which is not correct.
Test failed!

Test create_shared_object: shm_open and ftruncate succeed, but mmap fails. (0/1)
Setup:
    shared_memory_t shm;
    bool ok = create_shared_object( &shm, "/some_random_file_name");

Function returned false, which is not correct.
Test failed!

Test destroy_shared_object: all system calls succeed. (0/1)
Setup:
    shared_memory_t shm;
    bool ok = create_shared_object( &shm, "/some_random_file_name");
    if (ok) destroy_shared_object( &shm );

Contents of shared memory control struct are not correct.
Observed name     = lWSwqXO6euXjXVu
Observed fd       = 0
NULL
Expected name     = lWSwqXO6euXjXVu
Expected fd       = -1
NULL
Test failed!