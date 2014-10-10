#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <json/json.h>
#include <curl/curl.h>
#include <sglib.h>

//Every exchange has its own names  prices, volume and time, specify them in the array of strings
//First element is reserved for the root element i.e. "ticker", "btc-usd" etc
//If there is no root just leave empty
char btce_lables[][20] = {"btc_usd","low", "high", "last", "vol_cur", "updated"};
char bitstamp_lables[][20] = {"", "low", "high", "last", "volume", "timestamp"};
char bitfinex_lables[][20] = {"","low", "high", "last_price", "volume", "timestamp"};
char korbit_lables[][20] = {"", "low", "high", "last", "volume", "timestamp"};
int force_exit = 0;

struct Ticker{
    double low;
    double high;
    double last;
    double vol;
    unsigned long time;
    Ticker *next; // is used to create a list
    Ticker *previous;
};

//---------------------------------------------------------------------------------------------------------
//Structure that stores
//(a) pointer to memory where the server's response is stored
//(b) occupued memory size
struct MemoryStruct {
    char *memory;
    size_t size;
};
//---------------------------------------------------------------------------------------------------------
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
//---------------------------------------------------------------------------------------------------------
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
//---------------------------------------------------------------------------------------------------------
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
//---------------------------------------------------------------------------------------------------------
void printTicker(const char exName[], const Ticker *ticker){
    printf("%s\t|\tlow: %6.2lf,\t\thigh: %6.2lf,\t\tlast: %6.2lf,\t\tvolume: %6.2lf,\t\ttime: %6.2ld\n", exName,
           ticker->low, ticker->high, ticker->last, ticker->vol, ticker->time);
}
//---------------------------------------------------------------------------------------------------------
void sighandler(int sig){
    force_exit = 1;
}
//---------------------------------------------------------------------------------------------------------
struct Ticker *allocate_ticker_record(){
    return (struct Ticker *)calloc(1, sizeof(struct Ticker));
}
//---------------------------------------------------------------------------------------------------------
#define TICKER_COMPARATOR(e1, e2) (e1->time - e2->time)
#define MAX_NUM_ELEMENTS 3
//returns pointer to the last element
//if no news data is received, retures NULL
Ticker  *update(char *url, Ticker  *tickerList, Ticker  *head, char labels[][20]){
    struct MemoryStruct response = {0};
    response.memory = (char *)malloc(1);
    unsigned long list_length = 0;
    Ticker *itr_search, *itr_temp, *itr = allocate_ticker_record();
    getTicker(url, &response);
    parseTicker(&response, itr, labels);
    SGLIB_DL_LIST_ADD_IF_NOT_MEMBER(struct Ticker, tickerList, itr, TICKER_COMPARATOR, previous, next, itr_search);
    SGLIB_DL_LIST_LEN(struct Ticker, tickerList, previous, next, list_length)
    if(list_length  > MAX_NUM_ELEMENTS){
        itr_temp = head->previous;
        SGLIB_DL_LIST_DELETE(struct Ticker, tickerList, head, previous, next);
        head = itr_temp;
    }
    
    free(response.memory);
    if(itr_search == NULL)
        return itr;
    else
        return NULL;

}

int main(int argc, char **argv){
    signal(SIGINT, sighandler);

   //Create pointers to lists that will store tick data
    Ticker  *btceTickerList     = allocate_ticker_record(), //First element is always zero
            *bitstampTickerList = allocate_ticker_record(),
            *bitfinexTickerList = allocate_ticker_record(),
            *korbitTickerList   = allocate_ticker_record(),
            *element_ptr;
    Ticker  *btceTickerList_head        = btceTickerList, //Initializer list's header
            *bitstampTickerList_head    = bitstampTickerList,
            *bitfinexTickerList_head    = bitfinexTickerList,
            *korbitTickerList_head      = korbitTickerList;

    do{

        element_ptr = update("https://btc-e.com/api/3/ticker/btc_usd", btceTickerList, btceTickerList_head, btce_lables);
        if(element_ptr != NULL) //To avoid printing the same data twice, make sure we got new data
            printTicker("BTC-E\t", element_ptr);
        
        element_ptr = update("https://www.bitstamp.net/api/ticker/", bitstampTickerList, bitstampTickerList_head,bitstamp_lables);
        if(element_ptr != NULL) //To avoid printing the same data twice, make sure we got new data
            printTicker("Bitstamp", element_ptr);
        
        element_ptr = update("https://api.bitfinex.com/v1/pubticker/btcusd", bitfinexTickerList, bitfinexTickerList_head, bitfinex_lables);
        if(element_ptr != NULL) //To avoid printing the same data twice, make sure we got new data
            printTicker("Bitfinex", element_ptr);
        
        element_ptr = update("https://api.korbit.co.kr/v1/ticker/detailed", korbitTickerList, korbitTickerList_head, korbit_lables);
        if(element_ptr != NULL) //To avoid printing the same data twice, make sure we got new data
            printTicker("Korbit\t", element_ptr);
  
    }while(!force_exit);

    //here we need to release memory allocated for lists
    
    return 0;
}





