#!/usr/local/bin/slate



Cgi out ; 'Content-type: text/html\r\n\r\n'.

Cgi xhtml inDocumentDo: [|:w|

    [|:db| db connectTo: '127.0.0.1:5432'.
    [db loginAs: 'timmy' password: 'timmy' &database: 'mydb'.
     Cgi xhtml inTag: 'p' do: [|:w| w nextPutAll: (db query: 'select * from users') printString].
     
     ] ensure: [db close]] applyWith: DB Postgres new.


] &head: [|:w| w inTag: 'title' print: 'No title'] 
  &attributes: {#xmlns -> 'http://www.w3.org/1999/xhtml'. #lang -> 'en'. #xml:lang -> 'en'}.

Cgi out ; '\n\n'.

