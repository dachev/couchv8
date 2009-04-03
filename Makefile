couchv8: couchdb.cpp
	g++ -o couchv8 couchdb.cpp -lv8

clean:
	rm couchv8