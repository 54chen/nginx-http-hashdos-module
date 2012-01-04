#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


static void * ngx_http_hashdos_create_loc_conf(ngx_conf_t *cf);
static char * ngx_http_hashdos_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t ngx_http_hashdos_init(ngx_conf_t *cf);

typedef struct {
    ngx_flag_t    enable;
    ngx_str_t     remote_ip;
} ngx_http_hashdos_loc_conf_t;

static ngx_command_t  ngx_http_hashdos_commands[] = {
    { ngx_string("hashdos"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_hashdos_loc_conf_t, enable),
      NULL },
      ngx_null_command
};


static ngx_http_module_t  ngx_http_accesskey_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_hashdos_init,                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_hashdos_create_loc_conf,       /* create location configuration */
    ngx_http_hashdos_merge_loc_conf         /* merge location configuration */
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
    return conf;
}

static char *
ngx_http_hashdos_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_hashdos_loc_conf_t  *prev = parent;
    ngx_http_hashdos_loc_conf_t  *conf = child;
    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_str_value(conf->remote_ip,prev->remote_ip,"$remote_addr");
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
    ngx_uint_t   i; 
    ngx_http_accesskey_loc_conf_t  *alcf;

    alcf = ngx_http_get_module_loc_conf(r, ngx_http_hashdos_module);

    if (!alcf->enable) {
        return NGX_OK;
    }

    ngx_str_t args = r->args;

    ngx_uint_t j=0,k=0,l=0;

    for (i = 0; i <= args.len; i++) {
        if ( ( i == args.len) || (args.data[i] == '&') ) {
            if (j > 1) { k = j; l = i; }
            j = 0;
        }
    }
    return NGX_OK;
}
