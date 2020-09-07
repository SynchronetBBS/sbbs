// Helpers for http_request
var Request = {
    // Query parameter p exists and first instance is of optional type t
    has_param: function (p) {
      return (Array.isArray(http_request.query[p]) && http_request.query[p].length);
    },
    // First instance of query parameter p, or undefined
    get_param: function (p) {
      if (Array.isArray(http_request.query[p]) && http_request.query[p].length) {
          return http_request.query[p][0];
      }
    },
    write_param: function (p) {
      if (Array.isArray(http_request.query[p]) && http_request.query[p].length) {
          write(http_request.query[p][0]);
      }
    }
};
