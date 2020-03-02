<?php
//03-02-2020
//started on 24-02-2020
// La app di Heroku si puo richiamare da browser con
//			https://rollingshut.herokuapp.com/
// Account Heroku:  dariomelucci@gmail.com   pwd:  Bg_142666
// Account GitHub:	attiliomelucci@libero.it pwd:  Bg142666    name: attilio26

/*API key = 1011464393:AAER7EhOiy2ygCAFEdu5jzzk6WTSn7Thfes

da browser request ->   https://rollingshut.herokuapp.com/register.php
           answer  <-   {"ok":true,"result":true,"description":"Webhook is already set"}
In questo modo invocheremo lo script register.php che ha lo scopo di comunicare a Telegram
l�indirizzo dell�applicazione web che risponder� alle richieste del bot.

da browser request ->   https://api.telegram.org/bot1011464393:AAER7EhOiy2ygCAFEdu5jzzk6WTSn7Thfes/getMe
           answer  <-   {"ok":true,"result":{"id":1011464393,"is_bot":true,"first_name":"rollingshut","username":"rollingshutbot","can_join_groups":true,"can_read_all_group_messages":false,"supports_inline_queries":false}}

riferimenti:
https://gist.github.com/salvatorecordiano/2fd5f4ece35e75ab29b49316e6b6a273
https://www.salvatorecordiano.it/creare-un-bot-telegram-guida-passo-passo/
*/
//------passaggio da getupdates a  WEBHOOK
//da browser request ->   https://api.telegram.org/bot1011464393:AAER7EhOiy2ygCAFEdu5jzzk6WTSn7Thfes/setWebhook?url=https://rollingshut.herokuapp.com/execute.php
//					 answer  <-   {"ok":true,"result":true,"description":"Webhook was set"}
//          From now If the bot is using getUpdates, will return an object with the url field empty.
//------passaggio da webhook a  GETUPDATES
//da browser request ->   https://api.telegram.org/bot1011464393:AAER7EhOiy2ygCAFEdu5jzzk6WTSn7Thfes/setWebhook?url=
//					 answer  <-   {"ok":true,"result":true,"description":"Webhook was deleted"}


$content = file_get_contents("php://input");
$update = json_decode($content, true);

if(!$update)
{
  exit;
}

$message = isset($update['message']) ? $update['message'] : "";
$messageId = isset($message['message_id']) ? $message['message_id'] : "";
$chatId = isset($message['chat']['id']) ? $message['chat']['id'] : "";
$firstname = isset($message['chat']['first_name']) ? $message['chat']['first_name'] : "";
$lastname = isset($message['chat']['last_name']) ? $message['chat']['last_name'] : "";
$username = isset($message['chat']['username']) ? $message['chat']['username'] : "";
$date = isset($message['date']) ? $message['date'] : "";
$text = isset($message['text']) ? $message['text'] : "";

// pulisco il messaggio ricevuto togliendo eventuali spazi prima e dopo il testo
$text = trim($text);
// converto tutti i caratteri alfanumerici del messaggio in minuscolo
$text = strtolower($text);

header("Content-Type: application/json");

//ATTENZIONE!... Tutti i testi e i COMANDI contengono SOLO lettere minuscole
$response = '';
$helptext = "List of commands :
---SERRANDA--- 
/ext_on    -> Lampada esterna accesa
/ext_off   -> Lampada esterna spenta 
/int_on    -> Lampada interna accesa
/int_off   -> Lampada interna spenta 
/apri  		 -> Apre serranda
/chiudi		 -> Chiude serranda
/serranda  -> Lettura stato serranda
/boil_on	 -> Caldaia Ferroli accendi
/boil_off  -> Caldaia Ferroli spegni
";

if(strpos($text, "/start") === 0 || $text=="ciao" || $text == "help"){
	$response = "Ciao $firstname, benvenuto   \n". $helptext; 
}

//<-- Comandi ai rele
//Comandi a Caldaia Ferroli
elseif(strpos($text,"boil_on")){
	$response = file_get_contents("http://dario95.ddns.net:28083/?a=6");
	$response = substr($response, strpos($response, "r><h2>") + 1);
}
elseif(strpos($text,"boil_off")){
	$response = file_get_contents("http://dario95.ddns.net:28083/?a=7");
	$response = substr($response, strpos($response, "r><h2>") + 1);
}
//Lampada esterna
elseif(strpos($text,"ext_on")){
	$response = file_get_contents("http://dario95.ddns.net:28083/?a=0");
	$response = substr($response, strpos($response, "r><h2>") + 1);
}
elseif(strpos($text,"ext_off")){
	$response = file_get_contents("http://dario95.ddns.net:28083/?a=1");
	$response = substr($response, strpos($response, "r><h2>") + 1);
}
//Lampada interna
elseif(strpos($text,"int_on")){
	$response = file_get_contents("http://dario95.ddns.net:28083/?a=2");
	$response = substr($response, strpos($response, "r><h2>") + 1);	
}
elseif(strpos($text,"int_off")){
	$response = file_get_contents("http://dario95.ddns.net:28083/?a=3");
	$response = substr($response, strpos($response, "r><h2>") + 1);	
}
//serranda
elseif(strpos($text,"apri")){
	$response = file_get_contents("http://dario95.ddns.net:28083/?a=4");
	$response = substr($response, strpos($response, "r><h2>") + 1);
}
elseif(strpos($text,"chiudi")){
	$response = file_get_contents("http://dario95.ddns.net:28083/?a=5");
	$response = substr($response, strpos($response, "r><h2>") + 1);
}
//<-- Lettura pagina web
elseif(strpos($text,"serranda")){   
	$response = file_get_contents("http://dario95.ddns.net:28083");
	$response = substr($response, strpos($response, "r><h2>") + 1);
}

//<-- Manda a video la risposta completa
elseif($text=="/verbose"){
	$response = "chatId ".$chatId. "   messId ".$messageId. "  user ".$username. "   lastname ".$lastname. "   firstname ".$firstname . "\n". $helptext ;		
  $response = $response. "\n\n Heroku + dropbox libero.it";	
}


else
{
	$response = "Unknown command!";			//<---Capita quando i comandi contengono lettere maiuscole
}

// la mia risposta � un array JSON composto da chat_id, text, method
// chat_id mi consente di rispondere allo specifico utente che ha scritto al bot
// text � il testo della risposta
$parameters = array('chat_id' => $chatId, "text" => $response);
$parameters["method"] = "sendMessage";
// imposto la keyboard
$parameters["reply_markup"] = '{ "keyboard": [["/boil_on \ud83d\udd34", "/boil_off \ud83d\udd35"],
["/ext_on", "/ext_off"],
["/apri \ud83d\udd34", "/chiudi  \ud83d\udd35"],
["/int_on", "/int_off"],
["/serranda \u2753"]], "one_time_keyboard": false, "resize_keyboard": true}';
// converto e stampo l'array JSON sulla response
echo json_encode($parameters);
?>