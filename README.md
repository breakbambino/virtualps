PSPERF2
=======
This is PSPERF V2 Software package.


Prerequisites
=============

## install NDN related Libraries

    $ sudo apt update
    $ sudo apt install libssl-dev libboost-all-dev libsqlite3-dev libpcap-dev

## install ndn-cxx 0.7.1

    $ git clone https://github.com/named-data/ndn-cxx
    $ cd ndn-cxx
    $ git checkout ndn-cxx-0.7.1
    $ ./waf configure
    $ ./waf
    $ sudo ./waf install
    $ sudo vi /etc/ld.so.conf
    $ sudo ldconfig

### ld.so.conf
~~~
include /etc/ld.so.conf.d/*.conf
/usr/local/lib/
~~~

## install NFD 0.7.1

    $ git clone --recursive https://github.com/named-data/NFD
    $ cd NFD
    $ git checkout NFD-0.7.1
    $ ./waf configure
    $ ./waf
    $ sudo ./waf install
    $ sudo cp /usr/local/etc/ndn/nfd.conf.sample /usr/local/etc/ndn/nfd.conf
    $ nfd-start

## install python-ndn

    $ git clone https://github.com/zjkmxy/python-ndn.git
    $ cd python-ndn 
    $ sudo apt-get install python3-setuptools
    $ python3 setup.py build
    $ sudo python3 setup.py install

## install python modules

    $ apt install python3-pip
    $ pip3 install pytest
    $ pip3 install pyyaml
    $ pip3 install redis
    $ pip3 install siphash

## install PSDCNv2

    $ git clone https://etrioss.kr/tuple/psdcnv2.git
    $ cd psdcnv2
    $ git checkout -t origin/release-B
    
    #edit psdcnv2.config

## run PSDCNv2
    $ cd scripts
    $ ./start-broker /rn-1
    # $ ./stop-broker
    
### psdcnv2.config
~~~
# PSDCNv2 configurations

# *-- Network --*

network: [                     # DHT Network
    /rn-1
]

# broker_prefix: /rn-1         # Per-broker prefix


# *-- Names --*

# names_provider: ProcNames()
# names_provider: RegexpNames()
names_provider: TrieNames()

# *-- Store and Storage --*

storage_provider: TableStorage()
# storage_provider: FileStorage('./psdcnv2data')
# Using Redis Storage
# storage_provider: RedisStorage(redis.StrictRedis())

cache_size: 100

clear_store: True


# *-- Logging --*

logger:
    level: info
    handlers:
        fileHandler: PSDCNv2.log
        StreamHandler: sys.stdout

~~~

## create face & add route
~~~
$ sudo nfdc face create remote udp://111.0.0.2
face-created id=268 local=udp4://111.0.0.1:6363 remote=udp4://111.0.0.2:6363 persistency=persistent reliability=off congestion-marking=on congestion-marking-interval=100ms default-congestion-threshold=65536B mtu=8800

$ sudo nfdc route add prefix /rn nexthop 268 cost 10
route-add-accepted prefix=/rn nexthop=268 origin=static cost=10 flags=child-inherit expires=never

$ sudo nfdc route add prefix /rn-1 nexthop 268 cost 10
route-add-accepted prefix=/rn-1 nexthop=268 origin=static cost=10 flags=child-inherit expires=never

~~~
    
## install YAML Library for PSPERF

    $ sudo apt install libyaml-cpp-dev rapidjson-dev
    

Running
=============

## make psperf

    $ git clone https://etrioss.kr/forerunner/psperf/
    $ cd psperf
    $ make

## testing psperf

    $ export NDN_LOG=psperf.*=DEBUG    # INFO, DEBUG, ERROR
    
    $ bin/testmanager -c conf/scenario-001-testmanager.yaml


## publisher 개별 실행
    $ export NDN_LOG=psperf.*=DEBUG
    $ cd ~/virualps/virtual-device
    $ ./bin/virtual-device -c test-001-virtual-device.yaml

    

## subscriber 개별 실행

    $ export NDN_LOG=psperf.*=DEBUG
    $ cd ~/virualps/virtual-user
    $ ./bin/virtual-user -c test-001-virtual-user.yaml

 

