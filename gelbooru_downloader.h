#ifndef GELBOORU_DOWNLOADER_H
#define GELBOORU_DOWNLOADER_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <curl/curl.h>

#define GELBOORU_HOST "https://gelbooru.com"
#define GELBOORU_DEFAULT_USER_AGENT "Mozilla/5.0 (X11; Linux x86_64; rv:146.0) Gecko/20100101 Firefox/146.0"
#define GELBOORU_DEFAULT_DOWNLOAD_DIR_PATH "gelbooru_downloads"


/*
    VECTOR
*/
typedef struct vector {
    void** data;
    int size;
    int capacity;
} vector;

vector* vector_create();
void    vector_destroy(vector* v);
int     vector_size(vector* v);
int     vector_push_back(vector* v, void *item);
void*   vector_pop_back(vector *v);
void*   vector_index(vector* v, int index);


/*
    THREAD SAFE QUEUE
*/
typedef struct TSQ_Node {
    void *data;
    struct TSQ_Node *next;
} TSQ_Node;

typedef struct ThreadSafeQueue {
    TSQ_Node *head;
    TSQ_Node *tail;
    int size;
    int closed;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} ThreadSafeQueue;

ThreadSafeQueue*    tsq_create();
void                tsq_destroy(ThreadSafeQueue *queue);
int                 tsq_size(ThreadSafeQueue *queue);
int                 tsq_closed(ThreadSafeQueue *queue);
void                tsq_close(ThreadSafeQueue *queue);
int                 tsq_push(ThreadSafeQueue *queue, void *data);
void*               tsq_pop(ThreadSafeQueue *queue);



/*
    PROGRESS BAR
*/
typedef struct ProgressBar {
    int progress;
    int max_progress;
    int bar_width;
    char *prefix_text;
    char *postfix_text;
    pthread_mutex_t mutex;
} ProgressBar;


ProgressBar*    ProgressBar_create(int bar_width);
void            ProgressBar_destroy(ProgressBar *bar);
void            ProgressBar_set_progress(ProgressBar* bar, int progress);
void            ProgressBar_set_max_progress(ProgressBar* bar, int max_progress);
void            ProgressBar_set_prefix_text(ProgressBar* bar, const char *text);
void            ProgressBar_set_postfix_text(ProgressBar* bar, const char *text);
void            ProgressBar_print(ProgressBar* bar);





/*
    GELBOORU
*/
typedef struct gelbooru_raw_data {
    char *data;
    size_t size;
} gelbooru_raw_data;


typedef struct gelbooru_tag {
    char *tag;
    int post_count;
} gelbooru_tag;


typedef struct gelbooru_thread_arg {
   int thread_id;
   struct gelbooru* gbooru;
   struct gelbooru_downloader_data* data;
} gelbooru_thread_arg;


typedef struct gelbooru_downloader_data {
    vector *tags;
    ThreadSafeQueue *download_queue;

    pthread_t *parser_thread;
    gelbooru_thread_arg *parser_arg;
    ProgressBar *parser_bar;

    int download_thread_count;
    pthread_t *downloader_threads;
    gelbooru_thread_arg *downloader_args;
    ProgressBar **downloader_bars; 

    pthread_t *progress_thread;
    gelbooru_thread_arg *progress_arg;
} gelbooru_downloader_data;


typedef struct gelbooru {
    char *user_agent;
    char *downloads_dir_path;
    int download_thread_count;
    int parser_sleep_ms;
    int downloader_sleep_ms;
    vector *img_formats;
} gelbooru;


gelbooru*   gelbooru_create(void);
void        gelbooru_destroy(gelbooru* gbooru);

gelbooru_downloader_data*   gelbooru_downloader_data_create(gelbooru* gbooru);
void                        gelbooru_downloader_data_destroy(gelbooru_downloader_data* data);

int     gelbooru_directory_exists(const char *dir_path);
int     gelbooru_file_exists(const char *path);
int     gelbooru_mkdir(const char *dir_path);

size_t              gelbooru_rawdata_write_curl_callback(void *contents, size_t size, size_t nmemb, void *userp);
gelbooru_raw_data*  gelbooru_get_request(gelbooru* gbooru, const char* url);
void                gelbooru_raw_data_free(gelbooru_raw_data* data);

void gelbooru_set_user_agent(gelbooru* gbooru, const char *user_agent);
void gelbooru_set_download_thread_count(gelbooru* gbooru, int count);
void gelbooru_set_downloads_dirpath(gelbooru* gbooru, const char* path);
void gelbooru_set_parser_sleep_ms(gelbooru* gbooru, int ms);
void gelbooru_set_downloader_sleep_ms(gelbooru* gbooru, int ms);

int     gelbooru_add_image_format(gelbooru* gbooru, const char *format);
vector* gelbooru_get_image_formats(gelbooru* gbooru);

char*   gelbooru_construct_tag_search_url(const char* query);
char*   gelbooru_construct_tags_query(vector* tags);
char*   gelbooru_construct_posts_page_url(vector* tags, int pid);
char*   gelbooru_construct_image_url(const char *hash, const char *format);
char*   gelbooru_construct_image_output_path(const char *outdir, const char *hash, const char *format);

vector* gelbooru_parse_tags(gelbooru_raw_data* raw_data);
vector* gelbooru_parse_image_hashes(gelbooru_raw_data* page_html);
int     gelbooru_parse_max_pid(gelbooru_raw_data* page_html);

vector* gelbooru_tag_search(gelbooru* gbooru, const char* query);
void    gelbooru_tag_list_free(vector* tags);

size_t  gelbooru_image_write_curl_callback(void *contents, size_t size, size_t nmemb, void *userp);
int     gelbooru_image_write_progress_curl_callback(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
int     gelbooru_download_image(gelbooru* gbooru, const char * hash, ProgressBar *bar);

void*   gelbooru_parser_thread_func(void *arg);
void*   gelbooru_downloader_thread_func(void *arg);
void*   gelbooru_progress_thread_func(void *arg);

void    gelbooru_download(gelbooru* gbooru, vector* tags);












#ifdef GELBOORU_DOWNLOADER_IMPLEMENTATION

/*
    GELBOORU
*/

/* Creates main gelbooru struct*/
gelbooru* gelbooru_create(void) {
    gelbooru* gbooru = (gelbooru*) malloc(sizeof(gelbooru));
    if (gbooru == NULL) {
        printf("Failed to allocate mem for gelbooru downloader\n");
        return NULL;
    }

    gbooru->user_agent = NULL;
    gbooru->downloads_dir_path = NULL;
    gbooru->download_thread_count = 1;
    gbooru->parser_sleep_ms = 500;
    gbooru->downloader_sleep_ms = 500;
    gbooru->img_formats = vector_create();
    if (gbooru->img_formats == NULL) {
        printf("Failed to create image formats vector\n");
        free(gbooru);
        return NULL;
    }
    return gbooru;
}

/* Destroy gelbooru struct */
void gelbooru_destroy(gelbooru* gbooru) {
    if (gbooru == NULL) return;
    free(gbooru->user_agent);
    free(gbooru->downloads_dir_path);

    // free formats
    for (int i = 0; i < vector_size(gbooru->img_formats); i++) {
        char *format = vector_index(gbooru->img_formats, i);
        free(format);
    }
    vector_destroy(gbooru->img_formats);
    free(gbooru);
}



/* Create gelbooru_downloader_data for download */
gelbooru_downloader_data* gelbooru_downloader_data_create(gelbooru* gbooru) {
    if (gbooru == NULL) return NULL;

    gelbooru_downloader_data *data = (gelbooru_downloader_data*) malloc(sizeof(gelbooru_downloader_data));
    if (data == NULL) {
        printf("Failed to allocate mem for downloader data\n");
        return NULL;
    }
    memset(data, 0, sizeof(gelbooru_downloader_data));

    // download queue
    data->download_queue = tsq_create();
    if (data->download_queue == NULL) {
        printf("Failed to create download queue\n");
        gelbooru_downloader_data_destroy(data);
        return NULL;
    }

    // parser
    data->parser_thread = (pthread_t*) malloc(sizeof(pthread_t));
    if (data->parser_thread == NULL) {
        printf("Failed to allocate mem for parser pthread\n");
        gelbooru_downloader_data_destroy(data);
        return NULL;
    }

    data->parser_arg = (gelbooru_thread_arg*) malloc(sizeof(gelbooru_thread_arg));
    if (data->parser_arg == NULL) {
        printf("Failed to allocate mem for parser arg\n");
        gelbooru_downloader_data_destroy(data);
        return NULL;
    }

    data->parser_bar = ProgressBar_create(50);
    if (data->parser_bar == NULL) {
        printf("Failed to create parser bar\n");
        gelbooru_downloader_data_destroy(data);
        return NULL;
    }

    // downloader
    data->download_thread_count = gbooru->download_thread_count;
    data->downloader_threads = (pthread_t*) malloc(sizeof(pthread_t) * data->download_thread_count);
    if (data->downloader_threads == NULL) {
        printf("Failed to allocate mem for downloader threads\n");
        gelbooru_downloader_data_destroy(data);
        return NULL;
    }

    data->downloader_args = (gelbooru_thread_arg*) malloc(sizeof(gelbooru_thread_arg) * data->download_thread_count);
    if (data->downloader_args == NULL) {
        printf("Failed to allocate mem for downloader args\n");
        gelbooru_downloader_data_destroy(data);
        return NULL;
    }

    data->downloader_bars = (ProgressBar**) malloc(sizeof(ProgressBar*) * data->download_thread_count);
    if (data->downloader_bars == NULL) {
        printf("Failed to allocate mem for downloader bars\n");
        gelbooru_downloader_data_destroy(data);
        return NULL;
    }

    for (int i = 0; i < data->download_thread_count; i++) {
        data->downloader_bars[i] = ProgressBar_create(50);
        if (data->downloader_bars[i] == NULL) {
            printf("Failed to create downloader bars\n");
            gelbooru_downloader_data_destroy(data);
            return NULL;
        }
    }

    // progress
    data->progress_thread = (pthread_t*) malloc(sizeof(pthread_t));
    if (data->progress_thread == NULL) {
        printf("Failed to allocate mem for progress thread\n");
        gelbooru_downloader_data_destroy(data);
        return NULL;
    }

    data->progress_arg = (gelbooru_thread_arg*) malloc(sizeof(gelbooru_thread_arg));
    if (data->progress_arg == NULL) {
        printf("Failed to allocate mem for progress arg\n");
        gelbooru_downloader_data_destroy(data);
        return NULL;
    }

    return data;
}

/* Destroy gelbooru_downloader_data */
void gelbooru_downloader_data_destroy(gelbooru_downloader_data* data) {
    if (data != NULL) {
        tsq_destroy(data->download_queue);
        ProgressBar_destroy(data->parser_bar);
        if (data->downloader_bars != NULL) {
            for (int i = 0; i < data->download_thread_count; i++) {
                ProgressBar_destroy(data->downloader_bars[i]);
            }
            free(data->downloader_bars);
        }
        
        free(data->parser_thread);
        free(data->parser_arg);

        free(data->downloader_threads);
        free(data->downloader_args);

        free(data->progress_thread);
        free(data->progress_arg);

        free(data);
    }
}



/*
    FS functions
*/

int gelbooru_directory_exists(const char *dir_path) {
    return access(dir_path, F_OK) == 0 ? 1 : 0;
}

int gelbooru_mkdir(const char *dir_path) {
    return mkdir(dir_path, 0777) == 0 ? 1 : 0;
}

int gelbooru_file_exists(const char *path) {
    return access(path, F_OK) == 0 ? 1 : 0;
}



/* Write callback for GET request */
size_t gelbooru_rawdata_write_curl_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    gelbooru_raw_data *raw_data = (gelbooru_raw_data*) userp;
    size_t realsize = size * nmemb;
    raw_data->data = (char*) realloc(raw_data->data, raw_data->size + realsize + 1);
    if (raw_data->data == NULL) {
        printf("Failed to realloc raw_data\n");
        return 0;
    }

    memcpy(raw_data->data + raw_data->size, contents, realsize);
    raw_data->size += realsize;
    raw_data->data[raw_data->size] = '\0';
    return realsize;
}

/* GET request */
gelbooru_raw_data* gelbooru_get_request(gelbooru* gbooru, const char* url) {
    if (gbooru == NULL || url == NULL) return NULL;

    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    if (curl == NULL) {
        printf("Failed to init curl\n");
        return NULL;
    }

    gelbooru_raw_data* raw_data = (gelbooru_raw_data*) malloc(sizeof(gelbooru_raw_data));
    if (raw_data == NULL) {
        printf("Failed to create raw data\n");
        curl_easy_cleanup(curl);
        return NULL;
    }
    raw_data->data = NULL;
    raw_data->size = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);   
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, gelbooru_rawdata_write_curl_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, raw_data);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, (gbooru->user_agent != NULL ? gbooru->user_agent : GELBOORU_DEFAULT_USER_AGENT));

    //printf("GET %s\n", url);
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        gelbooru_raw_data_free(raw_data);
        curl_easy_cleanup(curl);
        return NULL;
    }


    curl_easy_cleanup(curl);
    return raw_data;
}

/* Free raw data*/
void gelbooru_raw_data_free(gelbooru_raw_data* data) {
    if (data != NULL) {
        free(data->data);
        free(data);
    }
}


/*
    Setters
*/

/* Set user agent */
void gelbooru_set_user_agent(gelbooru* gbooru, const char *user_agent) {
    if (gbooru == NULL) return;

    char *new_user_agent = (char*) malloc(strlen(user_agent) + 1);
    if (new_user_agent == NULL) {
        printf("Failed to allocate mem for new user agent\n");
        return;
    }

    free(gbooru->user_agent);
    gbooru->user_agent = new_user_agent;
}
/* Set download thread count */
void gelbooru_set_download_thread_count(gelbooru* gbooru, int count) {
    if (gbooru == NULL) return;

    gbooru->download_thread_count = count > 1 ? count : 1;
}
/* Set output dir */
void gelbooru_set_downloads_dirpath(gelbooru* gbooru, const char* path) {
    if (gbooru == NULL) return;

    char *new_path = strdup(path);
    if (new_path == NULL) {
        printf("Failed to allocate mem for new download path\n");
        return;
    }
    free(gbooru->downloads_dir_path);
    gbooru->downloads_dir_path = new_path;
}
/* Set parser sleeps */
void gelbooru_set_parser_sleep_ms(gelbooru* gbooru, int ms) {
    if (gbooru != NULL) return;
    gbooru->parser_sleep_ms = ms > 100 ? ms : 100;
}
/* Set downloader sleeps */
void gelbooru_set_downloader_sleep_ms(gelbooru* gbooru, int ms) {
    if (gbooru != NULL) return;
    gbooru->downloader_sleep_ms = ms > 100 ? ms : 100;
}



/*
    Image formats
*/

/* Add image format */
int gelbooru_add_image_format(gelbooru* gbooru, const char *format) {
    if (gbooru == NULL) {
        return -1;
    }

    char *copy = strdup(format);
    if (copy == NULL) return -1;

    if (vector_push_back(gbooru->img_formats, copy) != 0) {
        free(copy);
        return -1;
    }
    return 0;
}

/* Get image formats */
vector* gelbooru_get_image_formats(gelbooru* gbooru) {
    if (gbooru == NULL) return NULL;
    return gbooru->img_formats;
}





/*
    Construct functions
*/


/*
    Construct tag search url
    Example "https://gelbooru.com/index.php?page=autocomplete2&type=tag_query&term=query"
*/
char* gelbooru_construct_tag_search_url(const char* query) {
    if (query == NULL || strlen(query) == 0) {
        printf("Empty query\n");
        return NULL;
    }

    char* encoded_query = curl_easy_escape(NULL, query, 0);
    char format_url[] = GELBOORU_HOST "/index.php?page=autocomplete2&type=tag_query&term=%s";
    int url_size = strlen(format_url) + strlen(encoded_query);
    char *url = (char*) malloc(url_size);
    if (url == NULL) {
        printf("Failed to allocate memory for tag search url\n");
        curl_free(encoded_query);
        return NULL;
    }

    sprintf(url, format_url, encoded_query);
    curl_free(encoded_query);
    return url;
}

/*
    Construct encoded tags query for posts page url
    Params: char* vector
    Example "tag1 tag2 ..." (before curl encoding)
*/
char* gelbooru_construct_tags_query(vector* tags) {
    if (tags == NULL || vector_size(tags) <= 0) return NULL;

    int total_length = 1;
    for (int i = 0; i < vector_size(tags); i++) {
        total_length += strlen((char*) vector_index(tags, i)) + 1;
    }
    char *tags_query = malloc(total_length);
    if (tags_query == NULL) {
        return NULL;
    }
    tags_query[0] = '\0';
    for (int i = 0; i < vector_size(tags); i++) {
        strcat(tags_query, (char*) vector_index(tags, i));
        strcat(tags_query, " ");
    }
    return tags_query;
}

/*
    Construct posts page url, example https://gelbooru.com/index.php?page=post&s=list&tags=tags&pid=pid
*/
char* gelbooru_construct_posts_page_url(vector* tags, int pid) {
    if (pid < 0) return NULL;
    char *tags_query = gelbooru_construct_tags_query(tags);
    if (tags_query == NULL) return NULL;

    char* encoded_query = curl_easy_escape(NULL, tags_query, 0);
    free(tags_query);

    char format_url[] = GELBOORU_HOST "/index.php?page=post&s=list&tags=%s&pid=%d";
    int url_size = strlen(format_url) + strlen(encoded_query) + 16;
    char *url = (char*) malloc(url_size);
    if (url == NULL) {
        printf("Failed to allocate memory for post page url\n");
        curl_free(encoded_query);
        return NULL;
    }

    sprintf(url, format_url, encoded_query, pid);
    curl_free(encoded_query);
    return url;
}

/*
    Construct image url with hash and format
*/
char* gelbooru_construct_image_url(const char *hash, const char *format) {
    if (hash == NULL || format == NULL) return NULL;

    char base_url[] = GELBOORU_HOST "/images/%s/%s/%s.%s";
    int url_size = strlen(base_url) + 4 + strlen(hash) + strlen(format);
    char *url = malloc(url_size);
    if (url == NULL) return NULL;

    char dir1[3], dir2[3];
    strncpy(dir1, hash, 2);
    strncpy(dir2, hash + 2, 2);
    dir1[2] = '\0';
    dir2[2] = '\0';

    sprintf(url, base_url, dir1, dir2, hash, format);
    return url;
}

/*
    Construct output image output path
*/
char* gelbooru_construct_image_output_path(const char *outdir, const char *hash, const char *format) {
    if (outdir == NULL || hash == NULL || format == NULL) return NULL;

    int path_size = strlen(outdir) + strlen(hash) + strlen(format) + 4;
    char *path = (char*) malloc(path_size);
    if (path == NULL) {
        return NULL;
    }
    sprintf(path, "%s/%s.%s", outdir, hash, format);
    return path;
}






/*
    Parse tags from raw JSON data
    Returns vector gelbooru_tag
*/
vector* gelbooru_parse_tags(gelbooru_raw_data* raw_data) {
    if (raw_data == NULL || raw_data->data == NULL) return NULL;
    
    const char *pattern = "\"value\":\"([^\"]+)\",\"post_count\":\"([0-9]+)\"";
    regex_t regex;
    regmatch_t matches[3];
    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        printf("Failed to compile regex\n");
        return NULL;
    }

    // tags vector
    vector *tags = vector_create();
    if (tags == NULL) {
        printf("Failed to create tags vector\n");
        regfree(&regex);
        return NULL;
    }

    char *cursor = raw_data->data;
    while (regexec(&regex, cursor, 3, matches, 0) == 0) {
        int len1 = matches[1].rm_eo - matches[1].rm_so;
        int len2 = matches[2].rm_eo - matches[2].rm_so;
        
        char *tag = (char*) malloc(len1 + 1);
        char *post_count = (char*) malloc(len2 + 1);
        if (tag == NULL || post_count == NULL) {
            free(tag);
            free(post_count);
            break;
        }
        strncpy(tag, cursor + matches[1].rm_so, len1);
        strncpy(post_count, cursor + matches[2].rm_so, len2);
        tag[len1] = '\0';
        post_count[len2] = '\0';

        gelbooru_tag *current_tag = (gelbooru_tag*) malloc(sizeof(gelbooru_tag));
        if (current_tag == NULL) {
            free(tag);
            free(post_count);
            break;
        }
        
        current_tag->tag = tag;
        current_tag->post_count = atoi(post_count);
        free(post_count);

        if (vector_push_back(tags, current_tag) != 0) {
            free(tag);
            free(current_tag);
            break;
        }
        
        cursor += matches[0].rm_eo;
    }

    regfree(&regex);
    return tags;
}

/*
    Parse image hashes from HTML raw data
    Returns char* vector
*/
vector* gelbooru_parse_image_hashes(gelbooru_raw_data* page_html) {
    if (page_html == NULL || page_html->data == NULL) return NULL;
    const char *pattern = "thumbnail_([a-f0-9]+)\\.jpg";
    regex_t regex;
    regmatch_t matches[2];

    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        printf("Failed to compile regex\n");
        return NULL;
    }

    vector* image_hash_list = vector_create();
    if (image_hash_list == NULL) {
        regfree(&regex);
        return NULL;
    }
    char *cursor = page_html->data;
    while (regexec(&regex, cursor, 2, matches, 0) == 0) {
        int len = matches[1].rm_eo - matches[1].rm_so;
        char *hash = malloc(len + 1);
        if (hash == NULL) break;

        strncpy(hash, cursor + matches[1].rm_so, len);
        hash[len] = '\0';
        if (vector_push_back(image_hash_list, hash) != 0) {
            free(hash);
            break;
        }
        cursor += matches[0].rm_eo;
    }
    regfree(&regex);
    return image_hash_list;
}

/*
    Parse max page id from HTML raw data
*/
int gelbooru_parse_max_pid(gelbooru_raw_data* page_html) {
    if (page_html == NULL || page_html->data == NULL) return -1;
    int max_pid = -1;
    const char *pattern = "pid=([0-9]+)";
    regex_t regex;
    regmatch_t pmatch[2];


    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        printf("Failed to compile regex\n");
        return max_pid;
    }

    const char *cursor = page_html->data;
    while (regexec(&regex, cursor, 2, pmatch, 0) == 0) {
        int len = pmatch[1].rm_eo - pmatch[1].rm_so;
        char *pid_str = malloc(len + 1);
        if (pid_str == NULL) {;
            regfree(&regex);
            return max_pid;
        }
        strncpy(pid_str, cursor + pmatch[1].rm_so, len);
        pid_str[len] = '\0';

        int current_pid = atoi(pid_str);
        if (current_pid > max_pid) max_pid = current_pid;
        
        free(pid_str);
        cursor += pmatch[0].rm_eo;
    }
    regfree(&regex);
    return max_pid;
}





/*
    Search tags
    Returns vector gelbooru_tag
*/
vector* gelbooru_tag_search(gelbooru* gbooru, const char* query) {
    if (gbooru == NULL) return NULL;
    if (query == NULL || strlen(query) == 0) {
        printf("Query string is empty\n");
        return NULL;
    }
    if (strlen(query) < 3) {
        printf("Query string is too short < 3\n");
        return NULL;
    }

    char* url = gelbooru_construct_tag_search_url(query);
    if (url == NULL) {
        printf("Failed to construct tag search url\n");
        return NULL;
    }

    gelbooru_raw_data *raw_data = gelbooru_get_request(gbooru, url);
    if (raw_data == NULL) {
        printf("Failed to GET %s\n", url);
        free(url);
        return NULL;
    }
    
    vector *tags = gelbooru_parse_tags(raw_data);
    
    free(url);
    gelbooru_raw_data_free(raw_data);
    return tags;
}

/*
    Destructor for vector gelbooru tag
*/
void gelbooru_tag_list_free(vector* tags) {
    if (tags != NULL) {
        for (int i = 0; i < vector_size(tags); i++) {
            gelbooru_tag* tag = vector_index(tags, i);
            free(tag->tag);
            free(tag);
        }
        free(tags);
    }
}






/*
    CURL image write callback 
*/
size_t gelbooru_image_write_curl_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    FILE* fp = (FILE*) userp;
    size_t written = fwrite(contents, size, nmemb, fp);
    return written;
}


/*
    CURL image write progress callback
*/
int gelbooru_image_write_progress_curl_callback(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    ProgressBar *bar = (ProgressBar*) p;
    if (dltotal > 0) {
        ProgressBar_set_max_progress(bar, dltotal);
        ProgressBar_set_progress(bar, dlnow);

        double dnowMB = (double) dlnow / (1024*1024);
        double dtotalMB = (double) dltotal / (1024*1024);
        char postfix[32];
        sprintf(postfix, "%4.1f / %4.1f %s", dnowMB, dtotalMB, "MB");
        ProgressBar_set_postfix_text(bar, postfix);
    }

    return 0;
}


/*
    Download image with hash
    Check added formats
    Return 0 if OK
*/
int gelbooru_download_image(gelbooru* gbooru, const char * hash, ProgressBar *bar) {
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    if (curl == NULL) {
        printf("Failed to init curl\n");
        return -1;
    }

    int success = -1;
    char prefix[128], postfix[32], *url, *output_path;
    FILE *fp;
    for (int i = 0; i < vector_size(gbooru->img_formats); i++) { // check all added formats
        // construct url
        url = gelbooru_construct_image_url(hash, vector_index(gbooru->img_formats, i));
        if (url == NULL) continue;

        // construct output path
        output_path = gelbooru_construct_image_output_path(
            gbooru->downloads_dir_path != NULL ? gbooru->downloads_dir_path : GELBOORU_DEFAULT_DOWNLOAD_DIR_PATH, 
            hash, 
            vector_index(gbooru->img_formats, i)
        );
        if (output_path == NULL) {
            free(url);
            break;
        }

        // update bar
        sprintf(prefix, "%-32s.%-5s", hash, vector_index(gbooru->img_formats, i));
        ProgressBar_set_prefix_text(bar, prefix);

        // if exists
        if (gelbooru_file_exists(output_path)) {
            success = 0;
            free(url);
            free(output_path);
            sprintf(postfix, "%-20s", "Exists");
            ProgressBar_set_postfix_text(bar, postfix);
            break;
        }

        // open
        fp = fopen(output_path, "wb");
        if (!fp) {
            free(url);
            free(output_path);
            sprintf(postfix, "%-20s", "Failed to open");
            ProgressBar_set_postfix_text(bar, postfix);
            break;
        }

        // curl
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, gelbooru_image_write_curl_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) fp);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, gbooru->user_agent != NULL ? gbooru->user_agent : GELBOORU_DEFAULT_USER_AGENT);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, gelbooru_image_write_progress_curl_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, bar);

        res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        // close
        fclose(fp);
        if (res == CURLE_OK && http_code == 200) {
            success = 0;
            free(url);
            free(output_path);
            break;
        } else {
            // remove if failed
            remove(output_path);
        }

        // cleanup
        free(url);
        free(output_path);
    }

    curl_easy_cleanup(curl);
    return success;
}



/*
    Parser thread function.
    Parsing all pages, push image hashes to download queue
*/
void* gelbooru_parser_thread_func(void *arg) {
    gelbooru_thread_arg* args = (gelbooru_thread_arg*) arg;
    if (args == NULL) {
        printf("Gelbooru parser thread: empty args\n");
        return NULL;
    }
    gelbooru* gbooru = args->gbooru;
    gelbooru_downloader_data* data = args->data;
    if (gbooru == NULL || data == NULL) {
        return NULL;
    }
    int pid = 0, max_pid = 0;
    char prefix[32], postfix[32];
    char *url;
    gelbooru_raw_data *raw_data;
    vector* image_hash_list;

    sprintf(prefix, "%-10s", "Parser");
    ProgressBar_set_prefix_text(data->parser_bar, prefix);
    while (1) {
        url = gelbooru_construct_posts_page_url(data->tags, pid);
        if (url == NULL) {
            sprintf(postfix, "Failed to construct url");
            ProgressBar_set_postfix_text(data->parser_bar, postfix);

            printf("Gelbooru parser thread: failed to construct posts page url\n");
            break;
        }
        
        raw_data = gelbooru_get_request(gbooru, url);
        if (raw_data == NULL) {
            sprintf(postfix, "Failed to GET");
            ProgressBar_set_postfix_text(data->parser_bar, postfix);

            free(url);
            break;
        }

        // parse max pid
        if (pid == 0) {
            max_pid = gelbooru_parse_max_pid(raw_data);
            if (max_pid < 0) {
                sprintf(postfix, "Failed to parse max pid");
                ProgressBar_set_postfix_text(data->parser_bar, postfix);

                printf("Gelbooru parser thread: Failed to parse max pid\n");
                free(url);
                gelbooru_raw_data_free(raw_data);
                break;
            }
            ProgressBar_set_max_progress(data->parser_bar, max_pid);
        }

        // parse image hashes
        image_hash_list = gelbooru_parse_image_hashes(raw_data);
        if (image_hash_list == NULL) {
            sprintf(postfix, "Failed to parse image hashes");
            ProgressBar_set_postfix_text(data->parser_bar, postfix);

            printf("Gelbooru parser thread: Failed to parse image hashes\n");
            free(url);
            gelbooru_raw_data_free(raw_data);
            break;
        }


        // push to queue
        for (int i = 0; i < vector_size(image_hash_list); i++) {
            char *hash = vector_index(image_hash_list, i);
            //printf("%s\n", hash);
            if (tsq_push(data->download_queue, hash) != 0) {
                printf("Gelbooru parser thread: Failed push to download queue\n");
                free(hash);
            }
        }


        free(url);
        vector_destroy(image_hash_list);
        gelbooru_raw_data_free(raw_data);

        sprintf(postfix, "%-7d / ~%-7d", pid, max_pid);
        ProgressBar_set_progress(data->parser_bar, pid);
        ProgressBar_set_postfix_text(data->parser_bar, postfix);

        pid += 42;
        if (pid > max_pid) break;
        usleep(gbooru->parser_sleep_ms * 1000);
    }

    //ProgressBar_set_postfix_text(data->parser_bar, "Finished");
    return NULL;
}

/*
    Downloader thread func
    Pops image from queue and download image
*/
void* gelbooru_downloader_thread_func(void *arg) {
    gelbooru_thread_arg* args = (gelbooru_thread_arg*) arg;
    if (args == NULL) {
        printf("Gelbooru parser thread: empty args\n");
        return NULL;
    }
    int thread_id = args->thread_id;
    gelbooru* gbooru = args->gbooru;
    gelbooru_downloader_data* data = args->data;
    if (gbooru == NULL || data == NULL) {
        return NULL;
    }

    ProgressBar *bar = data->downloader_bars[thread_id];
    char prefix[32], postfix[32];
    char *image_hash;
    sprintf(prefix, "%s %3d", "Downloader", thread_id);
    ProgressBar_set_prefix_text(bar, prefix);
    while (1) {
        image_hash = tsq_pop(data->download_queue);
        if (image_hash == NULL) break;
        
        if (gelbooru_download_image(gbooru, image_hash, bar) != 0) {
            //ProgressBar_set_postfix_text(bar, "Failed");
        }

        free(image_hash);
        sprintf(postfix, "%-15s", "Sleep...");
        ProgressBar_set_postfix_text(bar, postfix);
        usleep(gbooru->downloader_sleep_ms * 1000);
    }
    sprintf(postfix, "%-10s", "Finished");
    ProgressBar_set_postfix_text(bar, postfix);
}

/*
    Progress thread func
    Prints download progress
*/
void* gelbooru_progress_thread_func(void *arg) {
    gelbooru_thread_arg* args = (gelbooru_thread_arg*) arg;
    if (args == NULL) {
        printf("Gelbooru parser thread: empty args\n");
        return NULL;
    }
    gelbooru* gbooru = args->gbooru;
    gelbooru_downloader_data* data = args->data;
    if (gbooru == NULL || data == NULL) {
        return NULL;
    }

    ProgressBar *parser_bar = data->parser_bar;
    ProgressBar **download_bars = data->downloader_bars;
    int lines_count = data->download_thread_count + 2;

    while (1) {
        printf("Images in queue: %d\n", tsq_size(data->download_queue));

        ProgressBar_print(parser_bar);
        printf("\n");

        for (int i = 0; i < data->download_thread_count; i++) {
            ProgressBar_print(download_bars[i]);
            printf("\n");
        }
        printf("\033[%dA", lines_count);

        usleep(100000); // 100ms
        if (tsq_closed(data->download_queue) && (tsq_size(data->download_queue) == 0)) {
            break;
        }
    }

    ProgressBar_print(parser_bar);
    printf("\n");
    for (int i = 0; i < data->download_thread_count; i++) {
        ProgressBar_print(download_bars[i]);
        printf("\n");
    }
}



/*
    Download all images multithreaded
    with progress
*/
void gelbooru_download(gelbooru* gbooru, vector* tags) {
    gelbooru_downloader_data* data = gelbooru_downloader_data_create(gbooru);
    if (data == NULL) {
        printf("Failed to create downloader data\n");
        return;
    }
    data->tags = tags;

    // chech dir
    if (gbooru->downloads_dir_path == NULL) {
        printf("Output dir not set, set default dir\n");
        gelbooru_set_downloads_dirpath(gbooru, GELBOORU_DEFAULT_DOWNLOAD_DIR_PATH);
    }
    if (!gelbooru_directory_exists(gbooru->downloads_dir_path)) {
        printf("Output dir not exists, creating ...\n");
        if (!gelbooru_mkdir(gbooru->downloads_dir_path)) {
            printf("Failed to create output dir\n");
            gelbooru_downloader_data_destroy(data);
            return;
        }
    }


    // parser
    gelbooru_thread_arg* parser_arg = data->parser_arg;
    parser_arg->thread_id = 0;
    parser_arg->gbooru = gbooru;
    parser_arg->data = data;
    if (pthread_create(data->parser_thread, NULL, gelbooru_parser_thread_func, parser_arg) != 0) {
        printf("Failed to create parser thread\n");
        gelbooru_downloader_data_destroy(data);
        return;
    }

    // downloaders
    for (int i = 0; i < data->download_thread_count; i++) {
        gelbooru_thread_arg* downloader_arg = &data->downloader_args[i];
        downloader_arg->thread_id = i;
        downloader_arg->gbooru = gbooru;
        downloader_arg->data = data;
        if (pthread_create(&data->downloader_threads[i], NULL, gelbooru_downloader_thread_func, downloader_arg) != 0) {
            printf("Failed to create parser thread\n");
            tsq_close(data->download_queue);
            gelbooru_downloader_data_destroy(data);
            return;
        }
    }

    // progress
    gelbooru_thread_arg *progress_arg = data->progress_arg;
    progress_arg->thread_id = 0;
    progress_arg->gbooru = gbooru;
    progress_arg->data = data;

    if (pthread_create(data->progress_thread, NULL, gelbooru_progress_thread_func, progress_arg) != 0) {
        printf("Failed to create progress thread\n");
        tsq_close(data->download_queue);
        gelbooru_downloader_data_destroy(data);
        return;
    }



    // parser
    pthread_join(*(data->parser_thread), NULL);
    tsq_close(data->download_queue);

    // downloader
    for (int i = 0; i < data->download_thread_count; i++) {
        pthread_join(data->downloader_threads[i], NULL);
    }

    // progress
    pthread_join(*(data->progress_thread), NULL);


    gelbooru_downloader_data_destroy(data);
}




/*
    VECTOR
*/
vector* vector_create() {
    vector* v = (vector*) malloc(sizeof(vector));
    if (v == NULL) return NULL;

    v->data = NULL;
    v->size = 0;
    v->capacity = 0;
    return v;
}

void vector_destroy(vector* v) {
    if (v != NULL) {
        free(v->data);
        free(v);
    }
}

int vector_size(vector* v) {
    if (v == NULL) return -1;
    return v->size;
}

int vector_push_back(vector* v, void *item) {
    if (v == NULL) return -1;

    if (v->size == v->capacity) {
        int new_cap = v->capacity == 0 ? 1 : v->capacity * 2;
        void **new_data = (void**) realloc(v->data, new_cap * sizeof(void*));
        if (new_data == NULL) return -1;

        v->data = new_data;
        v->capacity = new_cap;
    }
    v->data[v->size] = item;
    v->size++;
    return 0;
}

void* vector_pop_back(vector *v) {
    if (v == NULL || v->size == 0) return NULL;
    v->size--;
    return v->data[v->size];
}

void* vector_index(vector* v, int index) {
    if (v == NULL) return NULL;
    return v->data[index];
}





/*
    THREAD SAFE QUEUE
*/

ThreadSafeQueue* tsq_create() {
    ThreadSafeQueue *queue = (ThreadSafeQueue*) malloc(sizeof(ThreadSafeQueue));
    if (queue == NULL) {
        return NULL;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->closed = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
    return queue;
}

void tsq_destroy(ThreadSafeQueue *queue) {
    if (queue == NULL) return;

    pthread_mutex_lock(&queue->mutex);
    TSQ_Node *current = queue->head;
    while (current != NULL) {
        TSQ_Node *temp = current;
        current = current->next;
        free(temp);
    }
    pthread_mutex_unlock(&queue->mutex);

    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
    free(queue);
}

int tsq_size(ThreadSafeQueue *queue) {
    if (queue == NULL) return 0;

    int size;
    pthread_mutex_lock(&queue->mutex);
    size = queue->size;
    pthread_mutex_unlock(&queue->mutex);
    return size;
}

int tsq_closed(ThreadSafeQueue *queue) {
    if (queue == NULL) return 1;

    pthread_mutex_lock(&queue->mutex);
    int closed = queue->closed;
    pthread_mutex_unlock(&queue->mutex);

    return closed;
}

void tsq_close(ThreadSafeQueue *queue) {
    if (queue == NULL) return;

    pthread_mutex_lock(&queue->mutex);
    queue->closed = 1;
    pthread_cond_broadcast(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
}

int tsq_push(ThreadSafeQueue *queue, void *data) {
    if (queue == NULL) return -1;

    TSQ_Node *new_node = (TSQ_Node*) malloc(sizeof(TSQ_Node));
    if (new_node == NULL) return -1;

    new_node->data = data;
    new_node->next = NULL;

    pthread_mutex_lock(&queue->mutex);
    if (queue->tail == NULL) {
        queue->head = new_node;
        queue->tail = new_node;
    } else {
        queue->tail->next = new_node;
        queue->tail = new_node;
    }
    queue->size++;
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

void* tsq_pop(ThreadSafeQueue *queue) {
    if (queue == NULL) return NULL;

    pthread_mutex_lock(&queue->mutex);
    while (queue->head == NULL && !queue->closed) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    if (queue->head == NULL) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }
    
    TSQ_Node *node = queue->head;
    void *data = node->data;
    queue->head = node->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    queue->size--;
    pthread_mutex_unlock(&queue->mutex);

    free(node);
    return data;
}





/*
    PROGRESS BAR
*/

ProgressBar* ProgressBar_create(int bar_width) {
    ProgressBar *progress_bar = (ProgressBar*) malloc(sizeof(ProgressBar));
    if (progress_bar == NULL) {
        return NULL;
    }

    progress_bar->progress = 0;
    progress_bar->max_progress = 100;
    progress_bar->bar_width = bar_width >= 10 ? bar_width : 10;
    progress_bar->prefix_text = NULL;
    progress_bar->postfix_text = NULL;
    pthread_mutex_init(&progress_bar->mutex, NULL);

    return progress_bar;
}

void ProgressBar_destroy(ProgressBar *bar) {
    if (bar != NULL) {
        free(bar->prefix_text);
        free(bar->postfix_text);
        pthread_mutex_destroy(&bar->mutex);
    }
    free(bar);
}

void ProgressBar_set_progress(ProgressBar* bar, int progress) {
    if (bar == NULL) return;

    pthread_mutex_lock(&bar->mutex);
    bar->progress = progress < bar->max_progress ? progress : bar->max_progress;
    pthread_mutex_unlock(&bar->mutex);
}

void ProgressBar_set_max_progress(ProgressBar* bar, int max_progress) {
    if (bar == NULL) return;

    pthread_mutex_lock(&bar->mutex);
    bar->max_progress = max_progress > 0 ? max_progress : 1;
    pthread_mutex_unlock(&bar->mutex);
}

void ProgressBar_set_prefix_text(ProgressBar* bar, const char *text) {
    if (bar == NULL || text == NULL) return;

    char *new_prefix = strdup(text); 
    if (new_prefix == NULL) return;

    pthread_mutex_lock(&bar->mutex);

    free(bar->prefix_text);
    bar->prefix_text = new_prefix;

    pthread_mutex_unlock(&bar->mutex);
}

void ProgressBar_set_postfix_text(ProgressBar* bar, const char *text) {
    if (bar == NULL || text == NULL) return;

    char *new_postfix = strdup(text); 
    if (new_postfix == NULL) return;

    pthread_mutex_lock(&bar->mutex);
    
    free(bar->postfix_text);
    bar->postfix_text = new_postfix;
    
    pthread_mutex_unlock(&bar->mutex);
}

void ProgressBar_print(ProgressBar* bar) {
    if (bar == NULL) return;

    pthread_mutex_lock(&bar->mutex);
    
    float ratio = (float) bar->progress / bar->max_progress;
    int filled_width = (int) (ratio * bar->bar_width);
    int percent = (int) (ratio * 100);

    printf("\r%s [", bar->prefix_text ? bar->prefix_text : "");
    for (int i = 0; i < bar->bar_width; i++) {
        if (i < filled_width) printf("#");
        else printf("-");
    }
    printf("] %3d%% %s", percent, bar->postfix_text ? bar->postfix_text : "");
    fflush(stdout);

    pthread_mutex_unlock(&bar->mutex);
}

#endif



#endif