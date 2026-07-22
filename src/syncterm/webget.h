#ifndef WEBGET_H
#define WEBGET_H

struct webget_request {
	const char *name;
	const char *uri;
	const char *cache_root;
	const char *cache_key;
	const char *output_name;
	bool initialized;
	char *state;
	char *msg;
	pthread_mutex_t mtx;
	size_t remote_size;
	size_t received_size;
	uint64_t cb_data;
};

bool iniReadHttp(struct webget_request *req);
bool webget_fetch(struct webget_request *req, bool force);
bool init_webget_req(struct webget_request *req, const char *cache_root, const char *name, const char *uri);
bool init_webget_file_req(struct webget_request *req, const char *cache_root, const char *name, const char *uri,
    const char *cache_key, const char *output_name);
void destroy_webget_req(struct webget_request *req);

#endif
