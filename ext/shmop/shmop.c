/*
   +----------------------------------------------------------------------+
   | PHP version 4.0                                                      |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2001 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.02 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_02.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Slava Poliakov (slavapl@mailandnews.com)                    |
   |          Ilia Alshanetsky (iliaa@home.com)                           |
   +----------------------------------------------------------------------+
 */
/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "php_shmop.h"
# ifndef PHP_WIN32
# include <sys/ipc.h>
# include <sys/shm.h>
#else
#include "tsrm_win32.h"
#endif


#if HAVE_SHMOP

#include "ext/standard/info.h"

#ifdef ZTS
int shmop_globals_id;
#else
php_shmop_globals shmop_globals;
#endif

int shm_type;

/* {{{ shmop_functions[] 
 */
function_entry shmop_functions[] = {
	PHP_FE(shmop_open, NULL)
	PHP_FE(shmop_read, NULL)
	PHP_FE(shmop_close, NULL)
	PHP_FE(shmop_size, NULL)
	PHP_FE(shmop_write, NULL)
	PHP_FE(shmop_delete, NULL)
	{NULL, NULL, NULL}	/* Must be the last line in shmop_functions[] */
};
/* }}} */

/* {{{ shmop_module_entry
 */
zend_module_entry shmop_module_entry = {
	"shmop",
	shmop_functions,
	PHP_MINIT(shmop),
	PHP_MSHUTDOWN(shmop),
	NULL,
	NULL,
	PHP_MINFO(shmop),
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_SHMOP
ZEND_GET_MODULE(shmop)
#endif

/* {{{ rsclean
 */
static void rsclean(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
	struct php_shmop *shmop = (struct php_shmop *)rsrc->ptr;

	shmdt(shmop->addr);
	efree(shmop);
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(shmop)
{
	shm_type = zend_register_list_destructors_ex(rsclean, NULL, "shmop", module_number);
	
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(shmop)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(shmop)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "shmop support", "enabled");
	php_info_print_table_end();
}
/* }}} */

/* {{{ proto int shmop_open (int key, int flags, int mode, int size)
   gets and attaches a shared memory segment */
PHP_FUNCTION(shmop_open)
{
	zval **key, **flags, **mode, **size;
	struct php_shmop *shmop;	
	struct shmid_ds shm;
	int rsid;
	int shmflg=0;

	if (ZEND_NUM_ARGS() != 4 || zend_get_parameters_ex(4, &key, &flags, &mode, &size) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long_ex(key);
	convert_to_string_ex(flags);
	convert_to_long_ex(mode);
	convert_to_long_ex(size);

	shmop = emalloc(sizeof(struct php_shmop));
	memset(shmop, 0, sizeof(struct php_shmop));

	shmop->key = Z_LVAL_PP(key);
	shmop->shmflg |= Z_LVAL_PP(mode);

	if (memchr(Z_STRVAL_PP(flags), 'a', Z_STRLEN_PP(flags))) {
		shmflg = SHM_RDONLY;
		shmop->shmflg |= IPC_EXCL;
	} 
	else if (memchr(Z_STRVAL_PP(flags), 'c', Z_STRLEN_PP(flags))) {
		shmop->shmflg |= IPC_CREAT;
		shmop->size = Z_LVAL_PP(size);
	}
	else {
		php_error(E_WARNING, "shmopen: access mode invalid");
		efree(shmop);
		RETURN_FALSE;
	}

	shmop->shmid = shmget(shmop->key, shmop->size, shmop->shmflg);
	if (shmop->shmid == -1) {
		php_error(E_WARNING, "shmopen: can't get the block");
		efree(shmop);
		RETURN_FALSE;
	}

	if (shmctl(shmop->shmid, IPC_STAT, &shm)) {
		efree(shmop);
		php_error(E_WARNING, "shmopen: can't get information on the block");
		RETURN_FALSE;
	}	

	shmop->addr = shmat(shmop->shmid, 0, shmflg);
	if (shmop->addr == (char*) -1) {
		efree(shmop);
		php_error(E_WARNING, "shmopen: can't attach the memory block");
		RETURN_FALSE;
	}

	shmop->size = shm.shm_segsz;

	rsid = zend_list_insert(shmop, shm_type);
	RETURN_LONG(rsid);
}
/* }}} */

/* {{{ proto string shmop_read (int shmid, int start, int count)
   reads from a shm segment */
PHP_FUNCTION(shmop_read)
{
	zval **shmid, **start, **count;
	struct php_shmop *shmop;
	int type;
	char *startaddr;
	int bytes;
	char *return_string;

	if (ZEND_NUM_ARGS() != 3 || zend_get_parameters_ex(3, &shmid, &start, &count) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long_ex(shmid);
	convert_to_long_ex(start);
	convert_to_long_ex(count);

	shmop = zend_list_find(Z_LVAL_PP(shmid), &type);	

	if (!shmop) {
		php_error(E_WARNING, "shmread: can't find this segment");
		RETURN_FALSE;
	}

	if (Z_LVAL_PP(start) < 0 || Z_LVAL_PP(start) > shmop->size) {
		php_error(E_WARNING, "shmread: start is out of range");
		RETURN_FALSE;
	}

	if ((Z_LVAL_PP(start)+Z_LVAL_PP(count)) > shmop->size) {
		php_error(E_WARNING, "shmread: count is out of range");
		RETURN_FALSE;
	}

	if (Z_LVAL_PP(count) < 0 ){
		php_error(E_WARNING, "shmread: count is out of range");
		RETURN_FALSE;
	}

	startaddr = shmop->addr + Z_LVAL_PP(start);
	bytes = Z_LVAL_PP(count) ? Z_LVAL_PP(count) : shmop->size-Z_LVAL_PP(start);

	return_string = emalloc(bytes);
	memcpy(return_string, startaddr, bytes);

	RETURN_STRINGL(return_string, bytes, 0);
}
/* }}} */

/* {{{ proto void shmop_close (int shmid)
   closes a shared memory segment */
PHP_FUNCTION(shmop_close)
{
	zval **shmid;
	struct php_shmop *shmop;
	int type;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &shmid) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	shmop = zend_list_find(Z_LVAL_PP(shmid), &type);

	if (!shmop) {
		php_error(E_WARNING, "shmclose: no such shmid");
		RETURN_FALSE;
	}
	zend_list_delete(Z_LVAL_PP(shmid));
}
/* }}} */

/* {{{ proto int shmop_size (int shmid)
   returns the shm size */
PHP_FUNCTION(shmop_size)
{
	zval **shmid;
	struct php_shmop *shmop;
	int type;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &shmid) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long_ex(shmid);

	shmop = zend_list_find(Z_LVAL_PP(shmid), &type);

	if (!shmop) {
		php_error(E_WARNING, "shmsize: no such segment");
		RETURN_FALSE;
	}

	RETURN_LONG(shmop->size);
}
/* }}} */

/* {{{ proto int shmop_write (int shmid, string data, int offset)
   writes to a shared memory segment */
PHP_FUNCTION(shmop_write)
{
	zval **shmid, **data, **offset;
	struct php_shmop *shmop;
	int type;
	int writesize;

	if (ZEND_NUM_ARGS() != 3 || zend_get_parameters_ex(3, &shmid, &data, &offset) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long_ex(shmid);
	convert_to_string_ex(data);
	convert_to_long_ex(offset);

	shmop = zend_list_find(Z_LVAL_PP(shmid), &type);

	if (!shmop) {
		php_error(E_WARNING, "shmwrite: error no such segment");
		RETURN_FALSE;
	}

	if ( Z_LVAL_PP(offset) > shmop->size ) {
		php_error(E_WARNING, "shmwrite: offset out of range");
		RETURN_FALSE;
	}

	writesize = (Z_STRLEN_PP(data)<shmop->size-Z_LVAL_PP(offset)) ? Z_STRLEN_PP(data) : shmop->size-Z_LVAL_PP(offset);	
	memcpy(shmop->addr+Z_LVAL_PP(offset), Z_STRVAL_PP(data), writesize);

	RETURN_LONG(writesize);
}
/* }}} */

/* {{{ proto bool shmop_delete (int shmid)
   mark segment for deletion */
PHP_FUNCTION(shmop_delete)
{
	zval **shmid;
	struct php_shmop *shmop;
	int type;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &shmid) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	convert_to_long_ex(shmid);

	shmop = zend_list_find(Z_LVAL_PP(shmid), &type);

	if (!shmop) {
		php_error(E_WARNING, "shmdelete: error no such segment");
		RETURN_FALSE;
	}

	if (shmctl(shmop->shmid, IPC_RMID, NULL)) {
		php_error(E_WARNING, "shmdelete: can't mark segment for deletion (are you the owner?)");
		RETURN_FALSE;
	}

	RETURN_TRUE;
}
/* }}} */

#endif	/* HAVE_SHMOP */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
