#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "apr.h"
#include "apr_signal.h"
#include "apr_strings.h"
#include "apr_network_io.h"
#include "apr_file_io.h"
#include "apr_time.h"
#include "apr_getopt.h"
#include "apr_general.h"
#include "apr_lib.h"
#include "apr_portable.h"
#include "apr_poll.h"
#include <hiredis/hiredis.h>
#include <json-c/json.h>

#include "vhosts_db_redis.h"

#define BUFSIZE (8192)
#define DEF_SOCK_TIMEOUT	(APR_USEC_PER_SEC * 30)
#define BUFFER_SIZE (256*1024) // 256kB
#define REDIS_SOCKET    "/var/run/redis/redis-webconf.sock"

char *redis_lookup_last = NULL;
char *redis_lookup_res = NULL;
redisContext *redis_context = NULL;

struct vhost_config *new_vhost_config (apr_pool_t * p) {
	struct vhost_config *conf = apr_pcalloc (p,sizeof (struct vhost_config));
	return conf;
}

void free_vhost_config(struct vhost_config *conf,apr_pool_t * p) {
	/*
    free(conf->uri);
    free(conf->vhost);
    free(conf->user);
    free(conf->directory);
    free(conf->mysql_socket);
    free(conf->php_mode);
    free(conf->php_config);
    free(conf->php_modules);
	*/
}

int vhost_parseconfig(const char *json_data,struct vhost_config *conf,apr_pool_t * p) {
	json_object *jpwd;
	json_object *jobj;
	json_object *jback;

	jobj = apr_pcalloc(p,BUFFER_SIZE);

	jpwd = json_tokener_parse(json_data);

    json_object_object_get_ex(jpwd, "host",&jobj);
	conf->uri = (char*) apr_pcalloc(p,strlen(json_object_get_string(jobj))+1);
	strcpy(conf->uri,json_object_get_string(jobj));

    json_object_object_get_ex(jpwd, "vhost",&jobj);
	conf->vhost = (char*) apr_pcalloc(p,strlen(json_object_get_string(jobj))+1);
	strcpy(conf->vhost,json_object_get_string(jobj));

    json_object_object_get_ex(jpwd, "user",&jobj);
	conf->user = (char*) apr_pcalloc(p,strlen(json_object_get_string(jobj))+1);
	strcpy(conf->user,json_object_get_string(jobj));

    json_object_object_get_ex(jpwd, "directory",&jobj);
	conf->directory = (char*) apr_pcalloc(p,strlen("/home/") + strlen(conf->user) + strlen(json_object_get_string(jobj))+2);
	sprintf(conf->directory,"/home/%s/public_html/%s",conf->user,json_object_get_string(jobj));
    //strcpy(conf->directory,json_object_get_string(jobj));

	json_object_object_get_ex(jpwd, "backend",&jback);
	//jback = json_object_get_object(jobj);

	json_object_object_get_ex(jback, "mysql_socket",&jobj);
	conf->mysql_socket = (char*) apr_pcalloc(p,strlen(json_object_get_string(jobj))+1);
    strcpy(conf->mysql_socket,json_object_get_string(jobj)); 

	json_object_object_get_ex(jback, "php_config",&jobj);
	conf->php_config = (char*) apr_pcalloc(p,strlen(json_object_get_string(jobj))+1);
    strcpy(conf->php_config,json_object_get_string(jobj)); 

	conf->cache = (char*) apr_psprintf(p,"%s|%s|%s|%s|%s|%s",
                                             conf->uri,
                                             conf->vhost,
                                             conf->user,
                                             conf->directory,
                                             conf->mysql_socket,
                                             conf->php_config);
	conf->added = 0;
	return 0;
}


int vhost_getconfig(const char *tenant, const char *host, struct vhost_config *conf,apr_pool_t * p) {
    char *json_data;

	redisReply *reply;
    struct timeval timeout = { 1, 500000 }; // 1.5 seconds

	redis_context = redisConnectUnixWithTimeout(REDIS_SOCKET, timeout);

	if (redis_context == NULL || redis_context->err) {
		redisFree(redis_context);
		return 1;
	}

	reply = redisCommand(redis_context,"GET %s/%s", "WEBHOST/v1", host);

	if (reply->type == REDIS_REPLY_STRING) {
		json_data = malloc(reply->len+1);
		memset(json_data,0,reply->len+1);
		strncpy(json_data,reply->str,reply->len+1);
	} else {
	    freeReplyObject(reply);
	    redisFree(redis_context);
        return 2;
	}

	freeReplyObject(reply);
	redisFree(redis_context);
	
    return vhost_parseconfig(json_data,conf,p);
}
