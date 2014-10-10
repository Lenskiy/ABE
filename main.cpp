#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <json/json.h>
#include <curl/curl.h>

//Every exchange has its own names  prices, volume and time, specify them in the array of strings
//First element is reserved for the root element i.e. "ticker", "btc-usd" etc
//If there is no root just leave empty
char bitstamp_lables[][20] = {"", "low", "high", "last", "volume", "timestamp"};
char btce_lables[][20] = {"btc_usd","low", "high", "last", "vol_cur", "updated"};
char bitfinex_lables[][20] = {"","low", "high", "last_price", "volume", "timestamp"};
char korbit_lables[][20] = {"", "", "", "last", "", "timestamp"};
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
void parseTicker(const struct MemoryStruct *response, Ticker *ticker, const char exchangeLabels[][20]){
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
    printf("%s\t|\t\t\tlow: %6.2lf,\thigh: %6.2lf,\tlast: %6.2lf,\tvolume: %6.2lf,\ttime: %6.2ld\n", exName,
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
    Ticker bitfinexTicker;
    Ticker korbitTicker;
    unsigned long lastTick1 = 0;
    unsigned long lastTick2 = 0;
    unsigned long lastTick3 = 0;
    unsigned long lastTick4 = 0;
    
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
        
        getTicker("https://api.bitfinex.com/v1/pubticker/btcusd", &response);
        parseTicker(&response, &bitfinexTicker, bitfinex_lables);
        if(lastTick3 != bitfinexTicker.time) //To avoid printing the same data twice, make sure we got new data
            printTicker("Bitfinex", &bitfinexTicker);
        lastTick3 = bitfinexTicker.time;
        
        getTicker("https://api.korbit.co.kr/v1/ticker", &response);
        parseTicker(&response, &korbitTicker, korbit_lables);
        if(lastTick4 != korbitTicker.time) //To avoid printing the same data twice, make sure we got new data
            printTicker("Korbit\t", &korbitTicker);
        lastTick4 = korbitTicker.time;
    
    }while(!force_exit);

    free(response.memory);
    
    return 0;
}





