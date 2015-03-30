#!/Users/mob/git/me/build/macosx-x64-release/bin/ejs
print("HTTP/1.0 200 OK
Content-Type: text/plain

" + serialize(App.env, {pretty: true}) + "
")