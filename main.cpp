#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <unistd.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdlib.h>

#include <json/json.h>

#include <curl/curl.h>
//Every exchange has its own names  prices, volume and time, specify them in the array of strings
//First element is reserved for the root element i.e. "ticker", "btc-usd" etc
//If there is no root just leave empty
char bitstamp_lables[6][10] = {"", "low", "high", "last", "volume", "timestamp"};
char btce_lables[6][10] = {"btc_usd","low", "high", "last", "vol_cur", "updated"};
int force_exit = 0;

struct Ticker{
    double low;
    double high;
    double last;
    double vol;
    unsigned long time;
};
//Structure that stores
//(a) pointer to memory where the server's response is stored
//(b) occupued memory size
struct MemoryStruct {
    char *memory;
    size_t size;
};

//Callback function that will be called to initialize memory with a HTTP server response
static size_t writeMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp){
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    //reallocate memory to store new reponse
    mem->memory = (char *)realloc(mem->memory, mem->size + realsize + 1);
    if(mem->memory == NULL) { /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }
    
    memcpy(mem->memory, contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    
    return realsize;
}

//Requests a html from a give URL
//Stores it in the memory, a pointer will be in chunk->memory
void getTicker(char* url, struct MemoryStruct *chunk){
    CURL *curl_handle;
    CURLcode res;

    
    curl_handle = curl_easy_init();
    if(curl_handle) {
        curl_easy_setopt(curl_handle, CURLOPT_URL, url); //set URL from where pull HTML
        //set callback function that is execture when HTML is received
        curl_easy_setopt(curl_handle,   CURLOPT_WRITEFUNCTION, writeMemoryCallback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)chunk); //set memory where HTML will be stored
        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl_handle);
        /* Check for errors */
        if(res != CURLE_OK){
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            return;
        }
        /* always cleanup */
        curl_easy_cleanup(curl_handle);
    }
}
//Pass memory where the response is stored, it will be parsed within this function
//Pass ticker to get poluted with extracted values
//Pass labels for a specific exchange
void parseTicker(const struct MemoryStruct *response, Ticker *ticker, const char exchangeLabels[6][10]){
    json_object *complete_query, *sub_query;
    complete_query = json_tokener_parse(response->memory);
    
    
    if((sub_query = json_object_object_get(complete_query, exchangeLabels[0])) == NULL)
        sub_query = complete_query;
    
    
    ticker->low =  json_object_get_double (  json_object_object_get(sub_query, exchangeLabels[1])  );
    ticker->high = json_object_get_double (  json_object_object_get(sub_query, exchangeLabels[2])  );
    ticker->last = json_object_get_double (  json_object_object_get(sub_query, exchangeLabels[3])  );
    ticker->vol =  json_object_get_double (  json_object_object_get(sub_query, exchangeLabels[4])  );
    ticker->time =  json_object_get_int64(  json_object_object_get(sub_query,  exchangeLabels[5])  );
    
    json_object_put(sub_query);
    json_object_put(complete_query);
}

void printTicker(const char exName[], const Ticker *ticker){
    printf("%s\t|\t\t\tlow: %4.2lf,\thigh: %4.2lf,\tlast: %4.2lf,\tvolume: %4.2lf,\ttime: %4.2ld\n", exName,
           ticker->low, ticker->high, ticker->last, ticker->vol, ticker->time);
}

void sighandler(int sig){
    force_exit = 1;
}

int main(int argc, char **argv){
    signal(SIGINT, sighandler);
    struct MemoryStruct response;
    Ticker bitstampTicker;
    Ticker btceTicker;
    unsigned long lastTick1 = 0;
    unsigned long lastTick2 = 0;

    do{
        getTicker("https://btc-e.com/api/3/ticker/btc_usd", &response);
        parseTicker(&response, &btceTicker, btce_lables);
        if(lastTick1 != btceTicker.time) //To avoid printing the same data twice, make sure we got new data
            printTicker("BTC-E\t", &btceTicker);
        lastTick1 = btceTicker.time;
        
        getTicker("https://www.bitstamp.net/api/ticker/", &response);
        parseTicker(&response, &bitstampTicker, bitstamp_lables);
        if(lastTick2 != bitstampTicker.time) //To avoid printing the same data twice, make sure we got new data
            printTicker("Bitstamp", &bitstampTicker);
        lastTick2 = bitstampTicker.time;
    }while(!force_exit);

    free(response.memory);
    
    return 0;
}



