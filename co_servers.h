#ifndef CO_SERVERS_H
#define CO_SERVERS_H

#define COS_INTERNAL_ERR -1
#define COS_FOUND 0
#define COS_NOT_FOUND 1

int search_corelated_servers(const char* lookfilepath, const char* lookfor, char** out_address);

#endif /* CO_SERVERS_H */