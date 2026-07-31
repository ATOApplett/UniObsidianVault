#include <stdio.h>
#include <stdlib.h>
#include "dbl_vector.h"

// Definitions
#define DV_INITIAL_CAPACITY 4
#define DV_GROWTH_FACTOR 1.25
// function definitions
void dv_init (dbl_vector_t* vec);
//struct
struct dbl_vector {
    size_t size;
    size_t capacity;
    double* data;
};

void dv_init( dbl_vector_t* vec ) {
    //double* new_vector = malloc(DV_INITIAL_CAPACITY * sizeof(double));
    vec->size = 0;
    vec->capacity = DV_INITIAL_CAPACITY;
    vec->data = malloc(vec->capacity * sizeof(double));
}

void dv_ensure_capacity( dbl_vector_t* vec, size_t new_size ) {
    if (vec->size != new_size) {
        size_t new_capacity = vec->capacity * DV_GROWTH_FACTOR;
        if (new_size <= new_capacity) {
            new_capacity = new_size;
        }
        double* new_vector = realloc(vec->data, new_capacity * sizeof(double));
        vec->data = new_vector;
        vec->capacity = new_capacity;
    }
}

void dv_destroy( dbl_vector_t* vec ) {
    
}

void dv_copy( dbl_vector_t* vec, dbl_vector_t* dest ) {
    // INSERT SOLUTION HERE
}

void dv_clear( dbl_vector_t* vec ) {
    // INSERT SOLUTION HERE
}

void dv_push( dbl_vector_t* vec, double new_item ) {
    // INSERT SOLUTION HERE
}

void dv_pop( dbl_vector_t* vec ) {
    // INSERT SOLUTION HERE
}

double dv_last( dbl_vector_t* vec ) {
    double result = NAN;
    // INSERT SOLUTION HERE
    return result;
}

void dv_insert_at( dbl_vector_t* vec, size_t pos, double new_item ) {
    // INSERT SOLUTION HERE
}

void dv_remove_at( dbl_vector_t* vec, size_t pos ) {
    // INSERT SOLUTION HERE
}

void dv_foreach( dbl_vector_t* vec, void (*callback)(double, void*), void* info ) {
    // INSERT SOLUTION HERE
}