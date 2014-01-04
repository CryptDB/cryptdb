#pragma once

/*
 *  config.h
 *
 *  Configuration parameters.
 *
 */

// if this bit is set, we are working with MySQL, else we are working with
// Postgres
#define MYSQL_S 1

/************* EVAL/DEBUGGING FLAGS ************/

//Flags for evaluation of different scenarios:
#define PARSING 0
//strawman where values are decrypted on the fly before being used
#define DECRYPTFIRST 0

#define ASSERTS_ON true


/******* VERBOSITY ****************/

//flag for debugging, particularly verbose
const bool VERBOSE_G = false;

const bool VERBOSE_KEYACCESS = true;
const bool VERBOSE_EDBProxy = true;
const bool VERBOSE_EDBProxy_VERY = true;
