login-auth Sample
===

This sample shows how to configure a simple app-based login using ESP.
This sample uses the a web form for entering username and password credentials and application logic
for controlling the authentication process.

This sample uses:

* Https for encryption of traffic for login forms
* Redirection to a login page and logged out page
* Redirection to use https for login forms and http once logged in
* Self-signed certificate. You should obtain a real certificate.
* Login username and password entry via web form
* Automatic session creation and management
* Username / password validation using the "config" file-based authentication store.
* Blowfish encryption for secure password hashing

Notes:
* This sample keeps the passwords in the esp.json. The test password was created via:

    esp user add joshua pass1

  You can use "esp user [add|compute|remove|show] username" to manage the user and password.

* The sample is setup to use the "config" auth store which keeps the passwords in the esp.json file. Note: the passwords
    are statically defined and cannot be changed at runtime. Use the "app" authentication store and see the login-database
    sample to enable modifying users and passwords at runtime. Alternatively, set the store to "system" if you wish to use passwords in the system password database (linux or macosx only).

* Session cookies are created to manage server-side session state storage and to optimize authentication.

* The sample creates three routes. A "public" route for the login form and required assets. This route
    does not employ authentication. A default route that requires authentication for access. And a
    action route for the login controller that processes the login and logout requests.

Requirements
---
* [Download ESP](https://www.embedthis.com/esp/download.html)

To run:
---
    esp

The server listens on port 4000 for HTTP traffic and 4443 for SSL. Browse to:

     http://localhost:4000/

This will redirect to SSL (you will get a warning due to the self-signed certificate).
Continue and you will be prompted to login. The test username/password is:

    joshua/pass1

Code:
---
* app.c - Application code managing authentication
* cache - Directory for cached ESP pages
* controllers - Directory for controllers
* documents/public - Web pages and resources accessible without authentication
* documents - Web pages requiring authentication for access
* [documents/index.esp](documents/index.esp) - Home page
* [documents/public/login.esp](documents/public/login.esp) - Login page
* [controllers/user.c](controllers/user.c) - User login controller code
* [esp.json](esp.json) - ESP configuration file

Documentation:
---

* [ESP Documentation](https://www.embedthis.com/esp/doc/index.html)
* [ESP Configuration](https://www.embedthis.com/esp/doc/users/config.html)
