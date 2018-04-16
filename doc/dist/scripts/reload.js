
    $(document).ready(function() {
        (function() {
            var r = new XMLHttpRequest();
            r.onreadystatechange = function() {
                if (r.readyState >= 3 && r.status == 200) {
                    window.location.reload();
                }
            }
            r.open("GET", "/reload-service", true);
            r.send();
        })();
    });

