Test dv_ensure_capacity. (0/1)
Setup example:
    dbl_vector_t vec;
    dv_init( &vec, 100.0 * rand() / RAND_MAX );

Invoking dv_ensure_capacity with size = 1
2 allocations have been made, but there should be 1
Memory allocation details follow:
   Observed: : freed block of 232 bytes
   Observed: : allocated block of 232 bytes

   Expected: : allocated block of 232 bytes
   Expected: : freed block of 0 bytes
Test failed.

Test dv_ensure_capacity. (0/1)
Setup example:
    dbl_vector_t vec;
    dv_init( &vec );

    srand(SEED);
    
    for ( int i = 0; i < DV_INITIAL_CAPACITY; i++ ) {
        dv_push( &vec, 100.0 * rand() / RAND_MAX );
    }

Invoking dv_ensure_capacity with size = 130
Observed capacity does not match expected value:
expected = 130, observed = 5
Test failed.

Test dv_ensure_capacity. (0/1)
Setup example:
    dbl_vector_t vec;
    dv_init( &vec );

    srand(SEED);
    
    for ( int i = 0; i < 10; i++ ) {
        dv_push( &vec, 100.0 * rand() / RAND_MAX );
    }

Pushing 17 items onto vector
Invoking dv_ensure_capacity with size = 114
Observed capacity does not match expected value:
expected = 114, observed = 22
Test failed.

Test dv_copy. (0/1)
Setup example:
    dbl_vector_t src;
    dv_init( &src );

    srand(SEED);
    
    for ( int i = 0; i < 10; i++ ) {
        dv_push( &src, 100.0 * rand() / RAND_MAX );
    }

    dbl_vector_t dest;
    dv_init( &dest );
    dv_copy( &src, &dest );

Pushing 17 items onto vector
Observed capacity does not match expected value:
expected = 17, observed = 5
Test failed.

Test dv_push. (0/1)
Setup example:
    dbl_vector_t vec;
    dv_init( &vec );

    srand(SEED);
    
    for ( int i = 0; i < 1; i++ ) {
        dv_push( &vec, 100.0 * rand() / RAND_MAX );
    }

Pushing 1 items onto vector
2 allocations have been made, but there should be 1
Memory allocation details follow:
   Observed: : freed block of 232 bytes
   Observed: : allocated block of 232 bytes

   Expected: : allocated block of 232 bytes
   Expected: : freed block of 0 bytes
Test failed.

Test dv_push. (0/1)
Setup example:
    dbl_vector_t vec;
    dv_init( &vec );

    srand(SEED);
    
    for ( int i = 0; i < DV_INITIAL_CAPACITY; i++ ) {
        dv_push( &vec, 100.0 * rand() / RAND_MAX );
    }

Pushing 4 items onto vector
5 allocations have been made, but there should be 1
Memory allocation details follow:
   Observed: : freed block of 232 bytes
   Observed: : freed block of 232 bytes
   Observed: : freed block of 232 bytes
   Observed: : freed block of 232 bytes
   Observed: : allocated block of 232 bytes

   Expected: : allocated block of 232 bytes
   Expected: : freed block of 0 bytes
   Expected: : freed block of 0 bytes
   Expected: : freed block of 0 bytes
   Expected: : freed block of 0 bytes
Test failed.

Test dv_push. (0/1)
Setup example:
    dbl_vector_t vec;
    dv_init( &vec );

    srand(SEED);
    int n = 20 + rand() % 10;
    
    for ( int i = 0; i < n; i++ ) {
        dv_push( &vec, 100.0 * rand() / RAND_MAX );
    }

Pushing 27 items onto vector
Observed capacity does not match expected value:
expected = 27, observed = 22
Test failed.

Test dv_insert_at. (0/1)
Setup example:
    dbl_vector_t vec;
    dv_init( &vec );
    
    srand( SEED );    
    size_t pos = 100 + rand() % 100;
    double val = 100.0 * rand() / RAND_MAX;
    dv_insert_at( &vec, pos, val );

Pushing 0 items onto vector
Inserting item 51.33 at position 137
Observed size does not match expected value:
expected = 1, observed = 0
Test failed.

Test dv_insert_at. (0/1)
Setup example:
    dbl_vector_t vec;
    dv_init( &vec );
    
    srand( SEED );    
    int n = 10 + rand() % 10;
    
    for ( int i = 0; i < n; i++ ) {
        dv_push( &vec, 100.0 * rand() / RAND_MAX );
    }
    
    size_t pos = 100 + rand() % 100;
    double val = 100.0 * rand() / RAND_MAX;
    dv_insert_at( &vec, pos, val );

Pushing 17 items onto vector
Inserting item 8.46 at position 114
Observed size does not match expected value:
expected = 18, observed = 17
Test failed.

Test dv_insert_at. (0/1)
Setup example:
    dbl_vector_t vec;
    dv_init( &vec );
    
    srand( SEED );    
    int n = 10 + rand() % 10;
    
    for ( int i = 0; i < n; i++ ) {
        dv_push( &vec, 100.0 * rand() / RAND_MAX );
    }
    
    size_t pos = 0;
    double val = 100.0 * rand() / RAND_MAX;
    dv_insert_at( &vec, pos, val );

Pushing 17 items onto vector
Inserting item 47.24 at position 0
10 allocations have been made, but there should be 9
Memory allocation details follow:
   Observed: : freed block of 232 bytes
   Observed: : freed block of 240 bytes
   Observed: : freed block of 248 bytes
   Observed: : freed block of 256 bytes
   Observed: : freed block of 264 bytes
   Observed: : freed block of 280 bytes
   Observed: : freed block of 296 bytes
   Observed: : freed block of 320 bytes
   Observed: : freed block of 344 bytes
   Observed: : allocated block of 344 bytes

   Expected: : freed block of 232 bytes
   Expected: : freed block of 240 bytes
   Expected: : freed block of 248 bytes
   Expected: : freed block of 256 bytes
   Expected: : freed block of 264 bytes
   Expected: : freed block of 280 bytes
   Expected: : freed block of 296 bytes
   Expected: : freed block of 320 bytes
   Expected: : allocated block of 344 bytes
   Expected: : freed block of 0 bytes
Test failed.

Test dv_insert_at. (0/1)
Setup example:
    dbl_vector_t vec;
    dv_init( &vec );
    
    srand( SEED );    
    int n = 10 + rand() % 10;
    
    for ( int i = 0; i < n; i++ ) {
        dv_push( &vec, 100.0 * rand() / RAND_MAX );
    }
    
    size_t pos = n / 3;
    double val = 100.0 * rand() / RAND_MAX;
    dv_insert_at( &vec, pos, val );

Pushing 17 items onto vector
Inserting item 47.24 at position 5
10 allocations have been made, but there should be 9
Memory allocation details follow:
   Observed: : freed block of 232 bytes
   Observed: : freed block of 240 bytes
   Observed: : freed block of 248 bytes
   Observed: : freed block of 256 bytes
   Observed: : freed block of 264 bytes
   Observed: : freed block of 280 bytes
   Observed: : freed block of 296 bytes
   Observed: : freed block of 320 bytes
   Observed: : freed block of 344 bytes
   Observed: : allocated block of 344 bytes

   Expected: : freed block of 232 bytes
   Expected: : freed block of 240 bytes
   Expected: : freed block of 248 bytes
   Expected: : freed block of 256 bytes
   Expected: : freed block of 264 bytes
   Expected: : freed block of 280 bytes
   Expected: : freed block of 296 bytes
   Expected: : freed block of 320 bytes
   Expected: : allocated block of 344 bytes
   Expected: : freed block of 0 bytes
Test failed.