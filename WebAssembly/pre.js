Module['print'] = (function () {
    var element = document.getElementById('output');
    if (element) element.value = ''; // clear browser cache
    return function (text) {
        if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join(' ');
        console.log(text);
        if (element) {
            element.value += text + '\n';
            element.scrollTop = element.scrollHeight; // focus on bottom
        }
    };
})();