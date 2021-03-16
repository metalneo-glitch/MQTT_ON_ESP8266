# MQTT_ON_ESP8266
Prima di scoprire TASMOTA, ero io a scrivere i programmi per i miei sensori. Se all'inizio mi divertivo, quando i sensori iniziavano a crescere di numero il divertimento calava. Sviluppando solo nel tempo libero non avevo molte occasioni per aggiornare i sensori, che comunque comportava il dover recarmi in ogni angolo della casa per smontarli, riprogrammarli, rimontarli e sperare di non aver mosso troppi fili che ne compromettessero il funzionamento. Appena ho iniziato a cercare di aggiungere una funzionalità di aggiornamento OTA ho scoperto TASMOTA che mi ha risolto tutti i miei problemi e mi ha permesso di concetrarmi sullo sviluppo dell'interfaccia del mio sistema domotico.

Voglio comunque condividere i miei programmi nel caso possano servire a qualcuno. Le cose secondo me più interessanti sono:

    Collegamento tramite WIFI
    Settaggio degli indirizzi IP
    Settaggio parametri server MQTT
    Pubblicazione dei dati sul topic MQTT corrispondente
    Sottoscrizione a topic MQTT
    Pubblicazione dati in formato JSON

Sono tutti programmi più o meno funzionanti che possono andare bene come esempio per altri progetti o per capire come funzionano. Ho cercato di riordinare un po' il codice per renderlo vagamente leggibile ma sicuramente non aggiornerò niente dato che mi appoggio a tasmota per la gestione dei sensori.
