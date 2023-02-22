# PA3 | TCP Caching Web Proxy | Hampton Walker

This assignment is an extension to PA2, where we implement caching to speed up future visits to websites. Sites with dynamic content are not cached. We also implement a "blocklist" that holds resolved hostnames and ip addresses that users are not permitted to visit while using the proxy. A 403 error will be displayed if a user attempts to navigate to a blocked site.

## Compiling & Usage

Compile with gcc (`gcc proxy.c -o proxy`) and run the executable with two arguments: `./proxy port timeout` where port is an ephemeral port and timeout is the amount of time in seconds that a file remains in the cache. Once the timeout expires, the file will not be served from the cache. If a user revisits the site, the file will be re-written to the cache and restart the timeout. If a user visits a site with a cached file, the cached file's timeout **will not** be reset.

With caching, some sites saw up to a 100x speedup for page loading. Most sites would see at least a 25x faster load time.

To view the cache in a web browser (preferably Firefox), you have to manually configure your proxy settings to use your proxy. Simply put the ip address of where your proxy is located along with the port number, and **be sure to not enable your proxy for https**. This proxy only supports HTTP 1.0/1.1 GET requests.

## Testing

I wrote a few bash scripts to test my proxy.

- [getall](./getall): Uses wget to recursively get all files to a server through your proxy and port (all 3 are command line arguments). I only really tested against the `netsys.cs.colorado.edu` server, as I figured that would be the case when it came around to my grade.
- [localTest](./localTest): Ordered Parallel Get test against my locally running PA2 web server, grabbing all graphics files and running a diff against them.
- [netsysTest](./netsysTest): Ordered Parallel Get test against the instructor's solution to PA2, grabbing all graphics files and running a diff against them.

I also found several other sites to manually test my web proxy and found great speedups with the cache with various file types. [dpgraph](http://dpgraph.com) is a good site to test the proxy with, along with [smplanet](http://www.smplanet.com) (note: I saw long load times at first for smplanet, but page loads were pretty instantaneous when loaded from my cache).

## Grade: 98/100

This was the highest score of the class, tied with a few other students. We all failed the same test case of not returning a 400 error on a the `netsys.cs.colorado.edu` site when the port specified was `88` - possibly because we didn't handle timeouts on connect properly, but I am unsure exactly why.
