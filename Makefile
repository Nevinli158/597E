all:
	g++ -g -o cpu_usage_monitor cpu_usage_monitor.cpp -I lib/boost_1_55_0 -L lib/boost_1_55_0/stage/lib -lboost_regex
