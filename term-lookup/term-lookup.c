#include <mysql/mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "term-lookup.h"
#include "wstring.h"

/*
open a database and return the connection;
and create table dict_term automatically if not exist
*/

void *term_lookup_open(char *dbname){
  MYSQL *conn;
  MYSQL_RES *res;
  MYSQL_ROW row;
  char *server = "localhost";
  char *user = "root";
  char *password = "123";
  char *database = dbname; //"search_engine_db";
  conn = mysql_init(NULL);
  /* Connect to database */
  if (!mysql_real_connect(conn, server,
        user, password, database, 0, NULL, 0)) {
     fprintf(stderr, "%s\n", mysql_error(conn));
     exit(1);
  }
  printf("database connected...\n");
  /* send SQL query */
  if (mysql_query(conn, "show tables")) {
     fprintf(stderr, "%s\n", mysql_error(conn));
     exit(1);
  }
  res = mysql_store_result(conn);
  /* go through all the tables in the database
  if the table dose not exist, then create it */
  char* table_name = "dict_term";
  bool exist = false;
  while ((row = mysql_fetch_row(res)) != NULL){
     if(strcmp(table_name, row[0]) == 0){
       exist = true;
     }
   }
   if(!exist){
     printf("create table dict_term\n");
     char* create_table = "create table dict_term (id int auto_increment, term varchar(50), score int, type varchar(5), primary key (id));";
     /* create table */
     if (mysql_query(conn, create_table)) {
        fprintf(stderr, "%s\n", mysql_error(conn));
        exit(1);
     }
   }
  mysql_free_result(res);
  return conn;
}

/*
if term exists in the database, return term id
otherwise, insert the term into the database.

The first parameter is the database connection.
*/

term_id_t term_lookup(void * handler, wchar_t *w_term){
  char* term = wstr2mbstr(w_term);
  MYSQL_RES *res;
  MYSQL_ROW row;
  MYSQL *conn = (MYSQL *)handler;
  char query[80];
  strcpy (query,"select id from dict_term where term = '");
  strcat(query, term);
  strcat(query, "'");
  if (mysql_query(conn, query)) {
     fprintf(stderr, "%s\n", mysql_error(conn));
     exit(1);
  }
  res = mysql_store_result(conn);
  row = mysql_fetch_row(res);

  if(row == NULL){

    // if the term does not exist in the database
    // insert the term into database and then return the id
    char insertion[80];
    strcpy(insertion,"insert into dict_term (term) values ('");
    strcat(insertion, term);
    strcat(insertion, "')");

    //char *insertion = "insert into dict_term (term) values (";
    //insertion = strcat(insertion, term);
    //insertion = strcat(insertion, ")");
    if (mysql_query(conn, insertion)) {
       fprintf(stderr, "%s\n", mysql_error(conn));
       exit(1);
    }
    if (mysql_query(conn, query)) {
       fprintf(stderr, "%s\n", mysql_error(conn));
       exit(1);
    }
    res = mysql_store_result(conn);
    row = mysql_fetch_row(res);
    mysql_free_result(res);
    return (term_id_t)atoi(row[0]);
  }else{
    // return the id of the term
    mysql_free_result(res);
    return atoi(row[0]);
  }
}

int term_lookup_flush(void *conn){
  // null
  return 0;
}

/*
close the database connection
*/
int term_lookup_close(void *conn){
  if(conn != NULL){
    mysql_close(conn);
  }
}
