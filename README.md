Utility commands for docker
===========================

This repository provides a set of useful commands for docker users.

**License:** GNU General Public Licence 3+

**Copyright:** University of Rennes 1 (2016)


--------------------------------------------------------------------------------------


docker-ssh
----------

**Push and pull docker images over ssh**

This is a wrapper around `docker save` and `docker load` for transferring images directly between two docker daemons (without going through a registry).

The remote daemon is identified with a URL, which can be:

  - a string identifying ssh account
    - _hostname_ 
    - _user@hostname_
    - _user@hostname:port_
  - a docker url
    - _unix:///var/run/mydocker.sock_
    - _tcp://1.2.3.4_
  - a ssh account followed by a docker url
    - _user@hostname:tcp://4.5.6.7_

The `push` command may work with multiple destinations (to deploy the same image to multiple nodes in one shot).

If the sending daemon supports the [`--exclude`](https://github.com/docker/docker/pull/9304) option in `docker save` ([latest patch available here](https://github.com/a-ba/docker-ce/tree/aba/partial-save)) this script automatically detects it and use it to avoid transmitting the layers that are already present in the receiving daemon (only the new layers are sent). Note that with multiple destinations, the resulting exclude list is the intersection of the lists given by all receiving daemons (all daemons receive the same data).

The command also allow translating image names (adding or removing a prefix). For example, with `docker push --add-prefix PFX/ HOST IMG` the resulting image in the remote daemon will be named `PFX/IMG`.

<pre>
usage:
    docker-ssh pull URL IMAGE [IMAGE ...]    

    docker-ssh push URL IMAGE [IMAGE ...]
    docker-ssh push URL [URL ...] . IMAGE [IMAGE ...]

positional arguments:
  URL                    url identifying the remote node: either a
                         docker url: unix://, tcp://, ... or a ssh
                         account: [USER@]HOST[:PORT][:DOCKER_URL]
  IMAGE                  docker image (or repo) to be transferred

optional arguments:
  --add-prefix PREFIX    prefix to be prepended to the image name
  --remove-prefix PREFIX prefix to be removed from the image name
  --local URL            use an arbitrary docker daemon instead of
                         the default local daemon
  -i IDENTITY            path of the private key for ssh authentication
</pre>


docker-runx
-----------

**Run a X11 application inside a docker container**

This is a wrapper around `docker run` that gives access to the host X11 and pulseaudio servers.

<pre>
usage:
    docker-runx [--trusted]  [ 'docker run' options ] IMAGE [ COMMAND ... ]
</pre>

Technically this commands provides the X11 and pulseaudio sockets in external volumes and sets DISPLAY & PULSE_SERVER accordingly.

The access to the X11 server is granted by generating a new authorization cookie, thus there is no need to undermine the security with a xhost command (like: `xhost local:` or worse: `xhost +`). Furthermore the cookie is **untrusted** by default, which means that X11 clients using it are [sandboxed](http://www.x.org/releases/X11R7.6/doc/xextproto/security.html) (they cannot steal data from other clients, implement a key logger,...).

There are two security modes:

 - **untrusted** (default) This is the safest and should work well with most boring desktop applications. Unfortunately most X11 extensions are incompatible (you won't have 3D acceleration, shared memory, fancy input devices, ...)

 - **trusted** (enabled with `--trusted`) In this mode the container processes have **full access** to the X11 server. All extensions are usable and there is no isolation with the other applications. Note that in this mode, the container is run with [`--ipc=host`](https://docs.docker.com/engine/reference/run/) to allow client-server communication using shared memory.


docker-rundev
-------------

**Run a 'build' command inside a container**

_(Note: i am looking for a better way of naming this command)_

This command is a wrapper for running a container with a command that:

 - processes files from a _source_ directory
 - writes its output into a _target_ directory 
 - possibly caches some data into a _cache_ directory (that can be reused when the command is run later)

<pre>
usage:
    docker-rundev [-h] [-s SOURCE] [-t TARGET] [-c CACHE] [ -- DOCKER_OPTIONS ] IMAGE [ COMMAND ... ]

optional arguments:
  -s SOURCE, --source SOURCE	host path of the source dir (mounted as /source with an overlay)
  -t TARGET, --target TARGET	host path of the target dir (mounted as /target)
  -c CACHE, --cache CACHE       host path of the cache dir (mounted as /cache)
</pre>

The typical usage is for building an executable from source files: the source files are provided in the _source_ directory, the newly built executable is written into the _target_ directory and it may cache cache some data in the _cache_ directory to speed up the subsequent builds (for example: the maven repository inside `~/.m2home` ->  you do not want to download all dependencies every time you rebuild the app).

Additionally `docker-devrun` provides two sweet features:

 - the docker container is run exactly with the same UID, GID and extra groups than the host user running `docker-devrun`, which means that the created files (in the _target_ and _cache_ volume) have the same uid/gid as the user running the job (you do not want your build process to produce file owned by root or by some random user). 

 - the _source_ directory is immutable. In the container it is mounted as a read-only volume but with an overlay layer, so that the build process is allowed to make changes. However these changes are thrown away when the container is destroyed. Only the changes written into the _target_ / _cache_ directories are persistent.

### Notes

 - requires [unionfs-fuse](https://github.com/rpodgorny/unionfs-fuse) (fuse-based union filesystem)
 - `user_allow_other` must be enabled in `/etc/fuse.conf`

docker-diff
-----------
**Generate a patch from the changes between the container and its underlying image**

<pre>
usage:
    docker-diff [-h] CONTAINER FILE [FILE ...]
</pre>

This command is useful for writing a Dockerfile. The typical use case is when you want to alter the default configuration provided by a package, but without making a full copy of the files in your context. The only data you want to store is your changes, i.e. the patch.

Unfortunately the native docker client does not provide a straightforward way to generate a patch from differences made inside a docker container (`docker diff` provides only a summary of the changes).


docker-nsenter
--------------

**Wrapper for nsenter that accepts a docker container as target**

<pre>
usage: docker-nsenter -t CONTAINER  &lt;nsenter args...&gt;

optional arguments:
  -t CONTAINER, --target CONTAINER
                        target docker container
</pre>

[nsenter](http://man7.org/linux/man-pages/man1/nsenter.1.html) is a very nice command for debugging live containers. It allows entering only a subset of the container namespaces, for example to run an admin command that is not installed inside the container:

<pre>
docker-nsenter -t mycontainer --net netstat -anp
</pre>


docker-flatten
--------------

**Flatten docker images**

<pre>
usage: docker-flatten [-h] [-t TAG] [--replace] [--backup SUFFIX]
                      IMAGE [IMAGE ...]

optional arguments:
  -t TAG, --tag TAG  tag for the resulting image
  --replace          replace the current tag
  --backup SUFFIX    keep a tag on the previous image (with SUFFIX appended to
                     the tag version)
  --ignore-unknown   silently ignore unknown images listed in the command line
  --max-layers NB    silently ignore images with no more than NB layers
</pre>

This command merges all layers from a given image into a single one.

The implementation is based on `docker export` and `docker import`. Because
this process implies creating a temporary container, the resulting image has
slight differences with the original image (eg. /etc/hostname, ...)


mininit
-------

**A minimal init command for running as PID 1 inside the container**

Such a command is necessary to get rid of zombie processes (they show up when the termination of subprocesses is not correctly handled by the apps running in the container)

http://blog.chazomatic.us/2014/06/18/multiple-processes-inside-docker/

<pre>
usage:
    mininit COMMAND [ ARGS ... ]
</pre>

mininit runs the COMMAND as PID 2 and stay in the background, there it:

 - reaps any incoming zombie process
 - forwards received signals (SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGUSR1, SIGUSR2) to PID 2
 - propagates the exit code of PID 2


docker-remove-zombies
---------------------

**Remove all stopped containers**

This is an alias for:
<pre>
    docker ps -aq --no-trunc -f status=exited -f status=created | xargs docker rm 
</pre>


docker-remove-untagged-images
-----------------------------

**Remove all images that are not tagged**

This is an alias for:
<pre>
    docker images --no-trunc -q -f dangling=true | xargs docker rmi
</pre>


docker-upgrade
--------------

**Upgrade docker images**

<pre>
usage: docker-upgrade [-h] [-f] [--ignore-unknown] [--http-proxy URL] [-q]
                      [-v]
                      IMAGE [IMAGE ...]

positional arguments:
  IMAGE             image name (with wildcard expansion)

optional arguments:
  -h, --help        show this help message and exit
  -f, --force       force committing a new image even if there is no reported
                    upgrades
  --ignore-unknown  silently ignore unknown images listed in the command line
  --http-proxy URL  value for the `http_proxy` environment variable to be set
                    in the upgrade container
  -q, --quiet       decrease verbosity
  -v, --verbose     increase verbosity
</pre>

This command is intended for security upgrades. For each image, it runs a
container and attempts to perform a system upgrade. If the upgrade is
effective, then a new image is committed, replacing the old one. It supports
distributions based on debian (apt), alpine (apk) and redhat (yum/dnf).
