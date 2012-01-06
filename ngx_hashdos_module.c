
/*
 * Copyright (C) 2012 54chen<czhttp@gmail.com>
 * http://54chen.com
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


static void * ngx_http_hashdos_create_loc_conf(ngx_conf_t *cf);
static char * ngx_http_hashdos_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t ngx_http_hashdos_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_hashdos_handler(ngx_http_request_t *r);
static void ngx_hashdos_request_body_handler(ngx_http_request_t *r);

typedef struct {
    ngx_flag_t    enable;
    ngx_int_t    body_max_count;
} ngx_http_hashdos_loc_conf_t;

typedef struct {
    ngx_flag_t    done:1;
    ngx_flag_t    waiting_more_body:1;
} ngx_http_post_read_ctx_t;

static ngx_command_t  ngx_http_hashdos_commands[] = {
    { ngx_string("hashdos"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_hashdos_loc_conf_t, enable),
      NULL },
      ngx_null_command
};


static ngx_http_module_t  ngx_http_hashdos_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_hashdos_init,                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_hashdos_create_loc_conf,       /* create location configuration */
    ngx_http_hashdos_merge_loc_conf         /* merge location configuration */
};

ngx_module_t  ngx_http_hashdos_module = {
    NGX_MODULE_V1,
    &ngx_http_hashdos_module_ctx,           /* module context */
    ngx_http_hashdos_commands,              /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

static void *
ngx_http_hashdos_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_hashdos_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_hashdos_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }
    conf->enable = NGX_CONF_UNSET;
    conf->body_max_count = 1000;
    return conf;
}

static char *
ngx_http_hashdos_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_hashdos_loc_conf_t  *prev = parent;
    ngx_http_hashdos_loc_conf_t  *conf = child;
    ngx_conf_merge_value(conf->enable, prev->enable, 1);
    ngx_conf_merge_value(conf->body_max_count,prev->body_max_count,1000);
    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_hashdos_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_hashdos_handler;
    return NGX_OK;
}

static ngx_int_t
ngx_http_hashdos_handler(ngx_http_request_t *r)
{
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,"[hashdos] start handler....");
    ngx_int_t                   rc;
    ngx_http_hashdos_loc_conf_t *alcf;
    ngx_http_post_read_ctx_t    *ctx;

    alcf = ngx_http_get_module_loc_conf(r, ngx_http_hashdos_module);

    if (!alcf->enable) {
        return NGX_OK;
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_hashdos_module);
    if (ctx != NULL) {
        if (ctx->done) {
            return NGX_DECLINED;
        }
        return NGX_DONE;
    }

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_post_read_ctx_t));

    if (ctx == NULL) {
        ngx_http_finalize_request(r, NGX_ERROR);
        return NGX_ERROR;
    }
    ngx_http_set_ctx(r, ctx, ngx_http_hashdos_module);

    rc = ngx_http_read_client_request_body(r, ngx_hashdos_request_body_handler);
    
    if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,"[hashdos] get body request...., rc is : %O " , rc);
        return rc;
    }

    if (rc == NGX_AGAIN) {
        ctx->waiting_more_body = 1;
        return NGX_DONE;
    }
    
    return NGX_DECLINED;
}

static void 
ngx_hashdos_request_body_handler(ngx_http_request_t *r)
{
    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,"[hashdos] body trasfering is over....");
    ngx_http_hashdos_loc_conf_t *alcf;
    ngx_int_t                  count,  limit;
    u_char                      ch,     *p;
    ngx_chain_t                 *cl;
    ngx_buf_t                   *buf,   *next;
    ngx_http_post_read_ctx_t    *ctx;

    r->read_event_handler = ngx_http_request_empty_handler;
    ctx = ngx_http_get_module_ctx(r, ngx_http_hashdos_module);
    ctx->done = 1;
    r->main->count--;

    if (r->request_body == NULL || r->request_body->bufs == NULL) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,"[hashdos] body bufs is null....");
        return ;
    }
    
    alcf = ngx_http_get_module_loc_conf(r, ngx_http_hashdos_module);
    if (alcf->body_max_count <= 0) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,"[hashdos] configure body_max_count <= 0, set limit to 1000");
        limit = 1000;
    } else {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,"[hashdos] configure body_max_count is %O", alcf->body_max_count);
        limit = alcf->body_max_count;
    }    

    count = 0;

    cl = r->request_body->bufs;
    buf = cl->buf;
    next = '\0';

    if (cl->next == NULL) {
        for (p = buf->pos; p < buf->last; p++){
            ch = *p;
            if(ch == '&'){
                count++;
            }
        }
    }
    if(cl->next != NULL){
        for (;cl;cl = cl->next) {
            next = cl->buf;

            if (next->in_file) {
                ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                        "[hashdos] in-file buffer found. aborted. "
                        "consider increasing your client_body_buffer_size "
                        "setting....");
                ctx->waiting_more_body = 0;
                ctx->done = 1;
                r->main->count--;
                return ;
            }

            for (p = next->pos; p < next->last; p++){
                ch = *p;
                if(ch == '&'){
                    count++;
                }
            }
        }
    }
    ++count;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,"[hashdos] parse request body params count is .... %O, limit is %O", count, limit);

    if(count >= limit){
        (void) ngx_http_discard_request_body(r);
        ngx_http_finalize_request(r, NGX_HTTP_REQUEST_ENTITY_TOO_LARGE);    
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "[hashdos] in rb->bfs -> client intended to send too large body: %O bytes, body size: %O, limit is: %O",
                      r->headers_in.content_length_n,count,limit);
        ctx->waiting_more_body = 0;
        ctx->done = 1;
        r->main->count--;
    }

    if (ctx->waiting_more_body) {
        ctx->waiting_more_body = 0;
        ngx_http_core_run_phases(r);
    }
}
