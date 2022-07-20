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
    $ export NDN_LOG=psperf.*=DEBUG    # INFO, DEBUG, ERROR
    
    $ bin/publisher -c conf/scenario-001-publisher.yaml
    

## subscriber 개별 실행

    $ export NDN_LOG=psperf.*=DEBUG    # INFO, DEBUG, ERROR
    
    $ bin/subscriber -c conf/scenario-001-subscriber.yaml

 

Configure
=============

## testmanager
~~~
Logging:
    Level: psperf.*=DEBUG # INFO, DEBUG, WARNING, ERROR

Scenario:
    Publisher:
        Enable: true
        Profile: publisher
    Subscriber:
        Enable: true
        Profile: subscriber

publisher:
    Execution: ./publisher
    Config: ./scenario-001-publisher.yaml
 
subscriber:
    Execution: ./subscriber
    Config: ./scenario-001-subscriber.yaml

~~~


## publisher
~~~
Config:
    Duration: 0
    Round: 10
    PAInterval: 100  # microseconds
    PDInterval: 100  # microseconds
Logging:
    Target: .  # dirpath
    Console: true
    File: false
    
Publisher:
    Random: true # 하위 단계 Layer 순서를 Random 
    Layer:
        Enable: true
        Prefix: Gu-${gu}
        PrefixRange : 1~10
        Random: true  # 하위 단계 Layer 순서를 Random
        Layer:
            # Enable: true
            Prefix : BDOT-${dong}
            PrefixRange : 1~5
            # temp, humidity, fine-dust, ultrafine-dust, wind-direction, 
            # wind-speed, rainfall, noise, vibration, ultraviolet-rays, 
            # floating-population
            Layer:
                Enable: true
                Prefix: temp
                PrefixRange : 1
                Type: Real
                DataRange : -10 ~ -40
            Layer:
                Prefix: humidity
                Type: Real
                DataRange : -10 ~ 40
            Layer:
                Prefix: fine-dust
                Type: Real
                DataRange : 5 ~ 40
            Layer:
                Prefix: ultrafine-dust
                Type: Real
                DataRange : 10 ~ 50
            Layer:
                Prefix: wind-direction
                Type: Real
                DataRange : 0~360
            Layer:
                Prefix: wind-direction
                Type: Real
                DataRange : 0~360
            Layer_:
                Prefix: wind-speed
                Type: Real
                DataRange : 0~100
            Layer:
                Prefix: rainfall
                Type: Real
                DataRange : 0~100
            Layer:
                Prefix: noise
                Type: Real
                DataRange : 0~100
            Layer:
                Prefix: vibration
                Type: Real
                DataRange : 0~100
            Layer:
                Prefix: ultraviolet-rays
                Type: Real
                DataRange : 0~100
            Layer:
                Prefix: floating-population
                Type: Real
                DataRange : 0~100
    Layer:
        Enable: true
        Prefix: Gu-${gu}
        PrefixRange : 11~16
        Layer:
            Enable: true
            Prefix : Disaster-${dong}
            PrefixRange : 1~5
            # river-water-level, flooding, water-velocity, manhole-water-level, 
            # surface-water-level , tide-level, steep-slope
            Layer:
                Enable: true
                Prefix: river-water-level
                Type: Real
                DataRange : 0 ~ 100    # 1 개면 고정
            Layer:
                Prefix: flooding
                Type: Real
                DataRange : 0 ~ 100
            Layer:
                Prefix: water-velocity
                Type: Real
                DataRange : 0 ~ 100
            Layer:
                Prefix: manhole-water-level
                Type: Real
                DataRange : 0 ~ 100
            Layer:
                Prefix: surface-water-level
                Type: Real
                DataRange : 0 ~ 100
            Layer:
                Prefix: tide-level
                Type: Real
                DataRange : 0 ~ 100
            Layer:
                Prefix: steep-slope
                Type: Real
                DataRange : 0 ~ 100
~~~
    

## subscriber
~~~
Config:
    Duration: 0
    StartDelay: 1000000
    STRepeat: 10000  # microsecond, ST Interest 전송 간격
    STInterval: 100  # microsecond, ST Interest 전송 간격
    SMRepeat: 10000 # microsecond, SM Interest 전송 간격
    SMInterval: 100 # microsecond, SM Interest 전송 간격
    SDRepeat: 10000 # microsecond, SD Interest 전송 간격
    SDInterval: 100 # microsecond, SD Interest 전송 간격
Logging:
    Target: .  # dirpath
    Console: true
    File: false

Subscriber:
    Layer:
        Enable: true
        Prefix: Gu-${no}
        PrefixRange : 1~10
        Layer:
            Enable: true
            Prefix : BDOT-${no}
            PrefixRange : 1~5
            Layer:
                Enable: true
                Prefix: #
                Fetch: -1  # 지정된 seq를 요청
    Layer_:
        Enable: true
        Prefix: Gu-${no}
        PrefixRange : 11~16
        Layer:
            Enable: true
            Prefix : BDOT-${no}
            PrefixRange : 1~5
            Layer:
                Enable: true   # default true
                Prefix: #
                Fetch: -1 # 지정되지 않거나 <0 면, 마지막 seq 1 개

~~~

## Broker 재실행
    $ ./stop-brokers
    $ ./start-broker

## pub-perf 실행
    $ export NDN_LOG=psperf.*=DEBUG    # INFO, DEBUG, ERROR
    
    $ bin/pub-perf -c conf/perf-001-publisher.yaml
    
### perf-001-publisher.yaml
~~~
Config:
    Duration: 0
    PA:
        Enable: true
        Interval: 10  # microseconds
    PU:
        Enable: false
        Interval: 10  # microseconds
    PD:
        Enable: false
        Round: 10
        Interval: 10  # microseconds
        GenerateInterval: 1000000  # microseconds
        ContentSize: 1024

Logging:
    Target: ./logs  # dirpath
    Console: true
    File: true

Publisher:
    Layer:
        Enable: true
        Prefix: Gu-${gu}
        PrefixRange : 1-2
        Random: true  # 하위 단계 Layer 순서를 Random
        Layer:
            # Enable: true
            Prefix : BDOT-${dong}
            PrefixRange : 1-2
            # temp, humidity, fine-dust, ultrafine-dust, wind-direction, 
            # wind-speed, rainfall, noise, vibration, ultraviolet-rays, 
            # floating-population
            Layer:
                Prefix: temp
                PrefixRange : 1
                Type: Real    # Integer, Real, String
                DataRange : -10 ~ -40
            Layer:
                Prefix: humidity
                Type: Real
                DataRange : -10 ~ 40
            Layer:
                Prefix: fine-dust
                Type: Real
                DataRange : 5 ~ 40
            Layer:
                Prefix: ultrafine-dust
                Type: Real
                DataRange : 10 ~ 50
            Layer:
                Prefix: wind-direction
                Type: Real
                DataRange : 0~360
            Layer:
                Prefix: wind-speed
                Type: Real
                DataRange : 0~100
            Layer:
                Prefix: rainfall
                Type: Real
                DataRange : 0~100
            Layer:
                Prefix: noise
                Type: Real
                DataRange : 0~100
            Layer:
                Prefix: vibration
                Type: Real
                DataRange : 0~100
            Layer:
                Prefix: ultraviolet-rays
                Type: Real
                DataRange : 0~100
            Layer:
                Prefix: floating-population
                Type: Real
                DataRange : 0~100
    Layer:
        Enable: false
        Prefix: Gu-${gu}
        PrefixRange : 11-16
        Layer:
            Enable: true
            Prefix : Disaster-${dong}
            PrefixRange : 1-10
            # river-water-level, flooding, water-velocity, manhole-water-level, 
            # surface-water-level , tide-level, steep-slope
            Layer:
                Enable: true
                Prefix: river-water-level
                Type: Real
                DataRange : 0 ~ 100    # 1 개면 고정
            Layer:
                Prefix: flooding
                Type: Real
                DataRange : 0 ~ 100
            Layer:
                Prefix: water-velocity
                Type: Real
                DataRange : 0 ~ 100
            Layer:
                Prefix: manhole-water-level
                Type: Real
                DataRange : 0 ~ 100
            Layer:
                Prefix: surface-water-level
                Type: Real
                DataRange : 0 ~ 100
            Layer:
                Prefix: tide-level
                Type: Real
                DataRange : 0 ~ 100
            Layer:
                Prefix: steep-slope
                Type: Real
                DataRange : 0 ~ 100

PacketOptions:
    DefaultOptions:
        Interest:
            Lifetime : 5000   # ms
            CanBePrefix: false
            MustBeFresh: True
    PA:
        Interest:
            Lifetime : 5000   # ms
    PU:
        Interest:
            Lifetime : 5000   # ms
    PD:
        Interest:
            Lifetime : 5000   # ms

~~~
## sub-perf 실행

    $ export NDN_LOG=psperf.*=DEBUG    # INFO, DEBUG, ERROR
    
    $ bin/sub-perf -c conf/perf-001-subscriber.yaml
### perf-001-subscriber.yaml
~~~
Config:
    Duration: 0
    ST:
        Enable: true
        Interval: 10  # microsecond, ST Interest 전송 간격
    SM:
        Enable: false
        RN: /rn-1
        Interval: 10
    SD:
        Enable: false
        RN: /rn-1
        Round: 10
        Interval: 10
        RequestInterval: 1000000  # microseconds

Logging:
    Target: ./logs  # dirpath
    Console: true
    File: true

Subscriber:
    Layer:
        Enable: true
        Prefix: Gu-${no}
        PrefixRange : 1-2
        Layer:
            Enable: true
            Prefix : BDOT-${no}
            PrefixRange : 1-2
            Layer:
                Enable: true
                Prefix: temp
                Fetch: -1  # 지정된 seq를 요청
            Layer:
                Prefix: humidity
                Fetch: -1
            Layer:
                Prefix: fine-dust
                Fetch: -1
            Layer:
                Prefix: ultrafine-dust
                Fetch: -1
            Layer:
                Prefix: wind-direction
                Fetch: -1
            Layer:
                Prefix: wind-speed
                Fetch: -1
            Layer:
                Prefix: rainfall
                Fetch: -1
            Layer:
                Prefix: noise
                Fetch: -1
            Layer:
                Prefix: vibration
                Fetch: -1
            Layer:
                Prefix: ultraviolet-rays
                Fetch: -1
            Layer:
                Prefix: floating-population
                Fetch: -1
    Layer:
        Enable: false
        Prefix: Gu-${no}
        PrefixRange : 11-16
        Layer:
            Enable: true
            Prefix : Disaster-${no}
            PrefixRange : 1-10
            Layer:
                Enable: true
                Prefix: river-water-level
                Fetch: -1  # 지정된 seq를 요청
            Layer:
                Enable: true
                Prefix: flooding
                Fetch: -1
            Layer:
                Enable: true
                Prefix: water-velocity
                Fetch: -1
            Layer:
                Prefix: manhole-water-level
                Fetch: -1
            Layer:
                Prefix: surface-water-level
                Fetch: -1
            Layer:
                Prefix: tide-level
                Fetch: -1
            Layer:
                Prefix: steep-slope
                Fetch: -1

PacketOptions:
    DefaultOptions:
        Interest:
            Lifetime : 5000   # ms
            CanBePrefix: false
            MustBeFresh: True
    ST:
        Interest:
            Lifetime : 5000   # ms
    SM:
        Interest:
            Lifetime : 5000   # ms
    SD:
        Interest:
            Lifetime : 5000   # ms
~~~
    
