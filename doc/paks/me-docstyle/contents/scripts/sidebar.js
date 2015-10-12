
$('.sidebar-launch').click(function() {
    var body = $('body');
    if (!body.hasClass('show-sidebar')) {
        body.addClass('show-sidebar');
        $('.sidebar').css('visibility', 'visible');
    } else {
        body.removeClass('show-sidebar');
        $('.sidebar').css('visibility', 'hidden');
    }
});
$('.sidebar a.item').click(function(e) {
    sessionStorage.setItem('scroll-top', $('.sidebar').scrollTop());
});
$(function() {
    var url = '<@= meta.top.join(meta.url.path) @>';
    var active = $('.sidebar a[href="' + url +'"]');
    active.addClass('active');
    $('.sidebar').scrollTop(sessionStorage.getItem('scroll-top'));
});
