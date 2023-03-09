# Generating the List of Root Certificates

The list of root certificates comes from Mozilla’s NSS root certificate store, 
which can be found [here](https://wiki.mozilla.org/CA/Included_Certificates).

The list can be downloaded and created by running the script mk-ca-bundle.pl 
that is distributed as a part of curl. 

Another alternative would be to download the finished list directly from the curl website: 
[CA certificates extracted from Mozilla](https://curl.se/docs/caextract.html)
