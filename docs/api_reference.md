# MainSEL API Reference
TODO Include Mainzelliste endpoints

## TOC

## Process Overview

 * Setup for linkage 
 * Initialization (I)
 * Linkage / Matching (L/M)
 * Computation (C)
 * Results / Status (S)

## Preliminaries

### Global SEL Identifier
Each ML has a global ID, which consists of the character set `[a-zA-Z0-9_]`
(i.e. alphanumeric plus '_',). This global ID is also registered at the
LinkageService for linkage ID (LID) management. Under this ID the LS stores the
Identifier Encryption Keys and each global ID is assigned to an API key.

### Authorization
The reference implementation uses a protocol based on HTTP BASIC authentication.
The APIKey passed in the initialization phase is transmitted as an HTTP header
in the following way: 
```
Authorization: apiKey apiKey="<APIKey>"
```
Thus the specification follows [rfc2617](http://www.ietf.org/rfc/rfc2617.txt) .

## TL;DR: User-Facing API Endpoints

## Setup for Linkage ID Generation

### MainSEL Registration
Each new MainSEL in the federation must register its global ID with the Linkage
Service (LS). The LS then generates a secret key for this party for LID
(re)encryption. For security reasons, this process must be deliberately
performed locally by the LS administrator and is not exposed via a REST API.

### InitIDs
Since the L.1 call assumes that LIDs already exist in the remote ML, they must
initially be generated for the remote party.

#### Request
The ML designated as remote sends a
**HTTP GET** request to the linkage service address
`https://{linkage-service}:{ls-port}/freshIDs/<ML_id>?count=n` to obtain n valid
IDs, where n is the size of the entire ML database. A ML can only request valid
IDs for itself.

### Response
For a successful request, "200 OK" is sent with the generated LIDs as a JSON
object in the response body.

In case of a failed authentication "401 Unauthorized" is sent.

In case of an invalid request syntax "400 Bad Request" is sent.

If the LS is in an error state, it will respond with "500 Internal Server Error"
or "503 Service Unavailable". In the latter case a recovery time is sent with
the "Retry-After" header if possible.

### Example
```
// GET Request http://<linkage-service>/freshIDs/<global-ML-id>?count=n
// Authentication via HTTP header
// Response: base64 encoded list of n new ids
{
'linkageIds': [
'Rm9vQmF...yCg==',
...
'QmFyQmF...6Cg=='
]
```

## SEL Initialization (I)
A Mainzelliste (ML) must initialize its local SecureEpilinker (SEL) once. If SEL
is restarted (update, system reboot, ...) this process must be repeated, since
SEL deliberately does not persist states. Each ML/SEL pair must run through this
process, it does not differ between local and remote system.

Table

### Step I.1: Local Initialization

#### Request
To establish a connection between ML and SEL, the ML initializer sends a
TLS-authenticated and secured **HTTP PUT** to
`https://{sel-host}:{sel-port}/init/local`.

Since the local API key is not configured until this step, this call is
unauthenticated.

The following data is transferred:

 1. global identifier of the main cell list ("localId")
 2. information about the local authentication
 3. information about the data service providing the patient data (in the reference implementation the main cell list).
   1. URL
   2. Other fields dependent on the data service such as caching information, pagesizes, etc.
 4. linkage configuration
   1. Algorthimusetype
      * "epilink"
   2. Score from which an entry is identified as a hit
   3. Score below which an entry is treated as a non-hit.
   4. Existing Exchange Groups
      1. Fields of the groups are identified by field names
   5. Information about the used fields
      1. Field name
      2. Frequency (see EpiLink paper)
      3. Error rate (see EpiLink Paper)
      4. Comparison type
         * "dice" for scoring on dice coefficients.
         * "binary" for comparison on exact matches
      5. Data type of fieldv.
         * "bitmask"
         * "integer"
         * "number
         * "string"
      6. Bit length of the field

The authentication information is passed as an object with an authentication
type and the type appropriate information:

 1. type
    * apiKey
 2. shared key

The field type specification, following the [JSON Schema - Draft 4
standard](https://tools.ietf.org/html/draft-zyp-json-schema-04), distinguishes
between "integer", which is an integer value, and "number", which can be an
integer or a floating point number. The decimal separator is a dot.

Fields of type "bitmask" are sent base64 encoded. The comparison type "dice" is
only valid for the data type "bitmask". The comparison type "binary" is not
valid for fields of the "bitmask" data type.

Note: The default bitlengths are as follows:

 1. for Integer the bit length is ceil( log2( maximum allowed value) ),
 2. for doubles the bit length is 64 bit according to IEEE,
 3. for strings the bit length is (maximum number of characters) * 8 and
 4. bitmasks have the Bloom filter length of 500 bit.

#### Response
If the connection is successfully established, SEL responds "204 No Content".

If a local configuration already exists, it is updated and "200 OK" is
transmitted with the body "Updated connection".

If the request is not valid, "400 Bad Request" is transmitted.

If SEL is in an error state, it will respond with "500 Internal Server Error" or
"503 Service Unavailable". In the latter case a recovery time is sent in the
"Retry-After" header if possible.

#### Example
```
// HTTP - PUT von Mainzelliste an https://sel.mitro.dkfz.de:1234/initLocal
{
  "localId": "TUD",
  "localAuthentication": {
    "authType": "apiKey",
    "sharedKey": "123abc"
  },
  "dataService": {
    "url": "https://ml.mitro.dkfz.de:8080/rest/api/getAllRecords"
  },
  "algorithm": {
    "algoType": "epilink",
    "threshold_match": 0.9,
    "threshold_non_match": 0.7,
    "exchangeGroups": [[
      "vorname",
    "nachname",
    "geburtsname"
    ]],
    "fields": [{
      "name": "vorname",
      "frequency": 0.000235,
      "errorRate": 0.01,
      "comparator": "dice",
      "fieldType": "bitmask",
      "bitlength": 500
    },
    {
      "name": "nachname","frequency": 0.0000271,
      "errorRate": 0.008,
      "comparator": "dice",
      "fieldType": "bitmask",
      "bitlength": 500
    },
    {
      "name": "geburtsname",
      "frequency": 0.0000271,
      "errorRate": 0.008,
      "comparator": "dice",
      "fieldType": "bitmask",
      "bitlength": 500
    },
    {
      "name": "geburtstag",
      "frequency": 0.0333,
      "errorRate": 0.005,
      "comparator": "binary",
      "fieldType": "number",
      "bitlength": 5
    },
    {
      "name": "geburtsmonat",
      "frequency": 0.0833,
      "errorRate": 0.002,
      "comparator": "binary",
      "fieldType": "integer",
      "bitlength": 4
    },
    {
      "name": "geburtsjahr",
      "frequency": 0.0286,
      "errorRate": 0.004,
      "comparator": "binary",
      "fieldType": "integer",
      "bitlength": 12
    },
    {
      "name": "plz",
      "frequency": 0.01,
      "errorRate": 0.04,
      "comparator": "binary",
      "fieldType": "integer",
      "bitlength": 17
    },
    {
      "name": "ort",
      "frequency": 0.01,
      "errorRate": 0.04,
      "comparator": "dice",
      "fieldType": "bitmask",
      "bitlength": 500
    },
    {
      "name": "versichertennummer",
      "frequency": 0.01,
      "errorRate": 0.04,
      "comparator": "binary",
      "fieldType": "string",
      "bitlength": 240
    }]
  }
}
// HTTP Reply 204 No Content
```

### Step I.2: Remote Initialization

#### Request
To establish a connection between the main cell list and secureEpiLink for a
pair relationship, the initializer of the main cell list sends a TLS
authenticated and secured **HTTP PUT** to
`https://{sel-host}:{sel-port}/init/{remote_id}`."remote_id" is the unique
identifier of the remote peer. The identifier "local" is
reserved for local initialization.

The following data is transferred:

 1. connection profile
    1. Remote URL
    2. Remote authentication
 2. linkage service information
    1. URL
    2. Authentication information
 3. `matchingAllowed` flag -- is the calculation of the set intersection
    cardinality allowed? Default: false

The authentication information is passed as an object with an authentication
type and the type appropriate information:

    1. type
       * apiKey
    2. shared key

#### Response
If the connection is successfully created, SEL responds "204 No Content".

If a connection with the given ID already exists, it is updated and "200 OK" is
sent with the body "Updated connection".

In case of a failed authentication "401 Unauthorized" is sent.

In case of an invalid request syntax "400 Bad Request" is sent.

If SEL is in an error state, it will respond with "500 Internal Server Error" or
"503 Service Unavailable". In the latter case a recovery time is sent with the
"Retry-After" header if possible.

#### Example
```
// HTTP - PUT von Mainzelliste an https://sel.mitro.dkfz.de:1234/initRemote/<remote-id>
{
  "connectionProfile": {
    "url": "http://sel.mhh.de:1664",
      "authentication": {
        "authType": "apiKey",
        "sharedKey": "xkcd123"
      }
  },
    "linkageService": {
      "url": "https://ls.mitro.dkfz.de/",
      "authentication": {
        "authType": "apiKey",
        "sharedKey": "bcd234"
      }
    },
    "matchingAllowed": true // may be omitted, defaults to 'false'.
}
// HTTP Reply 204 No Content
```
### Step I.3a: testRemote

#### Request
#### Response
#### Example

### Step I.3b: testLinkageService

#### Request
#### Response
#### Example

## Record Linkage (L)
This process links (a) local record(s) against the remote database. This process
can occur any number of times in succession once process I (initialization) has
been successfully executed.


|      | Name              | Description                                                                                       | Caller           | Callee | API Endpoint at Callee   |
|------|-------------------|---------------------------------------------------------------------------------------------------|------------------|--------|--------------------------|
| L.1a | linkRecord        | The ML creates a record linkage job by providing a single record and the result callback address  | ML               | SEL    | /linkRecord/<remote_id>  |
| L.1b | linkRecords       | The ML creates a record linkage job by providing multiple records and the result callback address | ML               | SEL    | /linkRecords/<remote_id> |
| L.6  | sendLinkageResult | Both SELs send their XOR shares to the linkage service. The resulting LID(s) are in the response  | local+remote SEL | LS     | /linkageResult           |
| L.7  | linkCallback      | SEL sends the LID received in L.6 to the given callback address                                   | SEL              | ML     | <callback_url>           |


### Step L.1a: linkRecord

#### Request
To request SEL to perform a (single record) record linkage with "remote_id", the
communicator of the main cell list sends a TLS authenticated and secured **HTTP
POST** to `https://{sel-host}:{sel-port}/linkRecord/{remote_id}`.

This call is not idempotent, multiple identical calls generate multiple linkage
requests.

The following data is transferred:

 1. callback information
    1. Callback URL
 2. data fields

Empty fields are encoded as "null", e.g. for JSON as the null data type.

#### Response
If successful, SEL replies "202 Accepted". In the "Location" header field, the
position of the status query of the linkage job is transmitted (cf. [Status
Monitoring](#Status-Monitoring-(S))).

If the transmitted fields do not match the configuration initialized in process
I, "401 Unauthorized" is transmitted.

If the request is made before the initialization has been performed, "400 Bad
Request" is replied.

If SEL is in an error state, it will respond with "500 Internal Server Error" or
"503 Service Unavailable". In the latter case a recovery time is sent with the
"Retry-After" header if possible.

#### Example
```
// HTTP POST von Mainzelliste an https://{sel-host}:{sel-port}:8081/linkRecord/dkfz
{
  "callback": {
    "url": "https://mlc.mitro.dkfz.de/linkCallback?idType=link%3ATUD%3ADKFZ&idString=randomRef",
  },
  "fields": {
    "vorname": "I07A1YM4aKNXlQ...", // base64 encoded 500-bit bloom filter
    "nachname": "5f3dPPzqJvw=...",
    "geburtsname": null,
    "geburtstag": 24,
    "geburtsmonat": 12,
    "geburtsjahr": 1864,
    "plz": 65432,
    "ort": "EFXma8cPVEA=...",
    "versichertennummer": "A123456781-1"
  }
}
// HTTP Reply 202 Accepted, Content-Location: /jobs/0012345823
```

### Step L.1b: linkRecords

#### Request
To request SEL to perform a (multiple records) record linkage with "remote_id",
the communicator of the main cell list sends a TLS authenticated and secured
**HTTP POST** to `https://{sel-host}:{sel-port}/linkRecords/{remote_id}`.

This call is not idempotent, multiple identical calls generate multiple linkage
requests.

The following data is transferred:

 1. callback information
    1. Callback URL
 2. records
    1. data fields
 3. number of records

Empty fields are encoded as "null", e.g. for JSON as the null data type.

#### Response
If successful, SEL replies "202 Accepted". In the "Location" header field, the
position of the status query of the linkage job is transmitted (cf. [Status
Monitoring](#Status-Monitoring-(S))).

If the
transmitted fields do not match the configuration initialized in process I, "401
Unauthorized" is transmitted.

If the request is made before the initialization
has been performed, "400 Bad Request" is replied.

If SEL is in an error state,
it will respond with "500 Internal Server Error" or "503 Service Unavailable".
In the latter case a recovery time is sent with the "Retry-After" header if
possible.

#### Example
```
// HTTP POST von Mainzelliste an https://{sel-host}:{sel-port}:8081/linkRecords/dkfz
{
  "callback": {
    "url": "https://mlc.mitro.dkfz.de/linkCallback?idType=link%3ATUD%3ADKFZ&idString=randomRef",
  },
    "total":1250,
    "toDate": 1232345512,
    "records": [{
      "fields": {
        "vorname": "I07A1YM4aKNXlQ...", // base64 encoded 500-bit bloom filter
        "nachname": "5f3dPPzqJvw=...",
        "geburtsname": null,
        "geburtstag": 24,
        "geburtsmonat": 12,
        "geburtsjahr": 1864,
        "plz": 65432,
        "ort": "EFXma8cPVEA=...",
        "versichertennummer": "A123456781-1"
      }},
    {"fields": {
                 "vorname": "I07A1YM4aKNXlQ...", // base64 encoded 500-bit bloom filter
                 "nachname": "5f3dPPzqJvw=...",
                 "geburtsname": null,
                 "geburtstag": 24,
                 "geburtsmonat": 12,
                 "geburtsjahr": 1864,
                 "plz": 65432,
                 "ort": "EFXma8cPVEA=...",
                 "versichertennummer": "A123456781-1"
               },
    ...
      ]
    }
// HTTP Reply 202 Accepted, Content-Location: /jobs/0012345823
```
### Step L.5: sendLinkageResult

#### Request
#### Response
#### Example

### Step L.6: linkCallback

#### Request
#### Response
#### Example

## Record "Matching", Patient Intersection Cardinality (M)

### Step M.1a: matchRecord

#### Request
#### Response
#### Example

### Step M.1b: matchRecords

#### Request
#### Response
#### Example

### Step M.6a: matchCallback

#### Request
#### Response
#### Example

### Step M.6b: matchsCallback

#### Request
#### Response
#### Example

## Computation (C)
### Step C.1: initMPC

### Step C.2: getAllRecords

#### Request
#### Response
#### Example

### Step C.3: secureEpiLink

## Status Monitoring (S)
This process can run in parallel to processes L and M and enables the current
processing status of one or all matching/linkage requests to be queried.

|     | Name       | Description                                                              | Caller    | Callee | API Endpoint at Callee |
|-----|------------|--------------------------------------------------------------------------|-----------|--------|------------------------|
| S.1 | jobStatus  | ML or user sends the job ID to SEL and receives a processing status back | ML / User | SEL    | /jobs/<job_id>         |
| S.2 | statusList | ML or user receives the status of all jobs                               | ML / User | SEL    | /jobs/list             |

### Step S.1: jobStatus

#### Request
The ML or user sends a **HTTP GET** request to the local SEL under the resource
`/jobs/<job-id>`.

#### Response
The ML receives the job status as a string in the response body.
Possible states are:

 * QUEUED
 * RUNNING
 * HOLD
 * FAULT
 * DONE

If no job with the given ID exists, the response body is empty.

### Step S.2: statusList

#### Request

The ML or user sends a **HTTP GET** request to the local SEL under the resource
`/jobs/list`.
#### Response

The ML receives all job status as a JSON object in the response body.
The JSON object has the form:
```
{
  "<job-id>": "<status>",
  ...
}
```
Possible states are:

 * QUEUED
 * RUNNING
 * HOLD
 * FAULT
 * DONE

If no job  exists, the JSON is empty.
