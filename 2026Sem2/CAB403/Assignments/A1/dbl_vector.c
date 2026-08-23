#include <stdio.h>
#include <stdlib.h>
#include "dbl_vector.h"

void dv_init( dbl_vector_t* vec ) {
    //double* new_vector = malloc(DV_INITIAL_CAPACITY * sizeof(double));
    vec->size = 0;
    vec->capacity = DV_INITIAL_CAPACITY;
    vec->data = malloc(vec->capacity * sizeof(double));
    return;
}

void dv_ensure_capacity( dbl_vector_t* vec, size_t new_size ) {
    if (vec == NULL) {
        return;
    }
    if (new_size <= vec->capacity) {
        return;
    }
    size_t new_capacity = vec->capacity * DV_GROWTH_FACTOR;
    if (new_capacity < new_size) {
        new_capacity = new_size;
    }
    vec->capacity = new_capacity;
    vec->data = realloc(vec->data, vec->capacity * sizeof(double));
}

void dv_destroy( dbl_vector_t* vec ) {
    vec->size = 0;
    vec->capacity = 0;
    free(vec->data);
    vec->data = NULL;
    return;
}

void dv_copy( dbl_vector_t* vec, dbl_vector_t* dest ) {
    if (vec == NULL || dest == NULL){
		return;
	}
    dv_ensure_capacity(dest, vec->size);
    dest->size = vec->size;
    for (size_t i = 0; i < vec->size; i++){
        dest->data[i] = vec->data[i];
    }
}

void dv_clear( dbl_vector_t* vec ) {
    if (vec == NULL){
		return;
	} else {
        vec->size = 0;
    }
}

void dv_push( dbl_vector_t* vec, double new_item ) {
    if (vec == NULL){
		return;
	}

    dv_ensure_capacity(vec, vec->size + 1);
    vec->data[vec->size] = new_item;
    vec->size++;
}

void dv_pop( dbl_vector_t* vec ) {
	if (vec == NULL){
		return;
	} else if (vec->size > 0){
        vec->size--;
	} else{
		vec->size = 0;
	}
}

double dv_last( dbl_vector_t* vec ) {
    double result = NAN;
    if (vec->size > 0){
        result = vec->data[vec->size - 1];
    }
    return result;
}

void dv_insert_at( dbl_vector_t* vec, size_t pos, double new_item ) {
    if (vec == NULL) {
        return;
    }
    if (pos > vec->size) {
        pos = vec->size;
    }
    dv_ensure_capacity( vec, vec->size + 1 );
    for (size_t i = vec->size; i > pos; i--) {
        vec->data[i] = vec->data[i - 1];
    }
    vec->data[pos] = new_item;
    vec->size++;
}

void dv_remove_at( dbl_vector_t* vec, size_t pos ) {
    if (pos < vec->size){
        for (size_t i = pos; i < vec->size - 1; i++){
            vec->data[i] = vec->data[i + 1];
        }
        vec->size--;
    } else {
        return;
    }
}

void dv_foreach( dbl_vector_t* vec, void (*callback)(double, void*), void* info ) {
    if (vec == NULL){
		return;
	}
    for (int i = 0; i < vec->size; i++){
        callback(vec->data[i], info);
    }
}