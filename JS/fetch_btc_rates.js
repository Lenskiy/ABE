var http = require('https');
var btc_e_usd = { host: 'btc-e.com', path: '/api/3/ticker/btc_usd',  extra: 'btc_usd'};
var bitstamp_usd = { host: 'www.bitstamp.net', path: '/api/ticker/', extra: '' };

function fetch_rates (response) {

  response.setEncoding("utf8");
  var json_response = '';
  var json_obj;

  response.on("data", function (data) {
    json_response += data;
  });

  response.on("end", function () {
    //console.log(json_response);
    json_obj = JSON.parse(json_response);
    console.log(json_obj);
  });
  response.on('error', console.error);
}


http.get(btc_e_usd, fetch_rates);
http.get(bitstamp_usd, fetch_rates);
