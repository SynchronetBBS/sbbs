#ifndef WEBGET_H
#define WEBGET_H

struct webget_request {
	const char *name;
	const char *uri;
	const char *cache_root;
	char *state;
	char *msg;
	pthread_mutex_t mtx;
	size_t remote_size;
	size_t received_size;
	uint64_t cb_data;
};

bool iniReadHttp(struct webget_request *req);
bool init_webget_req(struct webget_request *req, const char *cache_root, const char *name, const char *uri);
void destroy_webget_req(struct webget_request *req);

#endif
