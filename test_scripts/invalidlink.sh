#curl -H "Content-Type: application/json" --data "{\"firstName\": \"bla\", \"lastName\": \"blubb\", \"age\": 5}" -X POST 'http://localhost:8080/init'
#curl -k -H "Content-Type: application/json" --data @testdata.json -X POST 'https://localhost:8080/init'
curl -v -k -H "Expect:" -H "Content-Type: application/json" --data @invalidinit.json -X POST 'https://localhost:8080/linkRecord/test_id'
#curl -v -k -H "Content-Type: application/json" --data @testdata3.json 'http://localhost:8080/init'