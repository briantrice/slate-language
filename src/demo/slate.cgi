#!/usr/local/bin/slate



Cgi out ; 'Content-type: text/html; charset=utf-8\r\n\r\n'.

Cgi xhtml inDocumentDo: [|:w|

    [|:db| db connectTo: '127.0.0.1:5432'.
    [db loginAs: 'timmy' password: 'timmy' &database: 'mydb'.
     w inTag: 'p' do: [|:w| w nextPutAll: (db query: 'select * from users') printString].
     
     ] ensure: [db close]] applyWith: DB Postgres new.


  w inTag: 'p' print: Cgi variables printString.

  w inTag: 'p' print: Cgi getValues printString.

  w inTag: 'p' print: Cgi postValues printString.

  w original ; '<p><form method="get" action="/cgi-bin/slate.cgi"><input type="text" name="user"/><input type="password" name="pw"/><input type="submit" name="submitbutton" value="getform" /></form></p>'.
  w original ; '<p><form method="post" action="/cgi-bin/slate.cgi"><input type="text" name="user"/><input type="password" name="pw"/><input type="submit" name="submitbutton" value="postform"/></form></p>'

] &head: [|:w| w inTag: 'title' print: 'No title'] 
  &attributes: {#xmlns -> 'http://www.w3.org/1999/xhtml'. #lang -> 'en'. #xml:lang -> 'en'}.

Cgi out ; '\n\n'.

