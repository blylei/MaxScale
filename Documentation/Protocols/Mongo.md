# MongoDB Protocol

The MongoDB protocol module provides access, subject to a number
limitations, to MariaDB databases for unalterered MongoDB applications.

TBW

# Client Library

Currently the only supported client library is version 3.6 of
[MongoDB Node.JS Driver](http://mongodb.github.io/node-mongodb-native/).

# Authentication

Currently no authentication is supported in the communication between
the MongoDB application and MaxScale. That is, the connection string
should look like
```
const uri = "mongodb://127.0.0.1:4711"
```
without a username and password.

The credentials to be used when MaxScale connects to the database on behalf
of the MongoDB application must be specified in the configuration as `user`
and `password` parameters to the MongoDB protocol module.
```
[MongoDB-Listener]
type=listener
protocol=mongodbclient
mongodbclient.user=the_username
mongodbclient.password=the_password
port=4711
...
```
Note that all MongoDB applications connecting to the MongoDB listener
port will use the same credentials when the MariaDB database is accessed;
in that respect it is not possible to distinguish one MongoDB application
from another.

# Parameters

## `user`

   * Type: string
   * Mandatory: true

Specifies the _user_ to be used when connecting to the backend. Note that the
same _user_/_password_ combination will be used for all Mongo clients connecting
to the same listener port.

## `password`

   * Type: string
   * Mandatory: true

Specifies the _password_ to be used when connecting to the backend. Note that the
same _user_/_password_ combination will be used for all Mongo clients connecting
to the same listener port.

## `on_unknown_command`

   * Type: enumeration
   * Mandatory: false
   * Values: `return_error`, `return_empty`
   * Default: `return_error`

Specifies what should happen in case a clients sends an unrecognized command.

Enumeration values:

   * `return_error`: An error document is returned.
   * `return_empty`: An empty document is returned.

## `auto_create_tables`

   * Type: boolean
   * Mandatory: false
   * Default: `true`

Specifies whether tables should automatically be created, as needed.

## `id_length`

   * Type: count
   * Mandatory: false
   * Range: `[24, 2048]`
   * Default: `24`

Specifies the length of the id column in tables that are automatically created.

## `insert_behavior`

   * Type: enumeration
   * Mandatory: false
   * Values: `as_mongodb`, `as_mariadb`
   * Default: `as_mongodb`

Enumeration values:

   * `as_mongodb`: Each document is inserted using a _separate_ INSERT statement
     and whether an error causes the remaining insertions to be aborted, depends
     on the value of `ordered` specified in the command document.
   * `as_mariadb`: If the value of `ordered` in the command document is `true`
     (the default) then all documents are inserted using a _single_ INSERT statement,
     that is, either all insertions succeed or none will.

## `cursor_timeout`

   * Type: duration
   * Mandatory: false
   * Default: 60s

Specifies how long a cursor can be idle, that is, not accessed, before it is
automatically closed.

# Databases and Tables

_Mongodbprotocol_ never creates databases, but they must be created manually.

Each Mongo _collection_ corresponds to a MariaDB table with the same name.
However, it is always possible to access a collection irrespective of whether
the corresponding table exists or not; it will simply appear to be empty.

Inserting documents into a collection, whose corresponding table does not
exist, succeeds, provided `auto_create_tables` is `true`, as the table will
in that case be created.

When _mongodbprotocol_ creates a table, it uses a statement like
```
CREATE TABLE name (id VARCHAR(24) NOT NULL UNIQUE, doc JSON)
```
where the length of the `VARCHAR` is specified by the value of `id_length`.

Note that _mongodbprotocol_ does not in any way verify that the table
corresponding to a collection being accessed or modified does indeed
have the expected columns `id` and `doc` of the expected types, but it
simply uses the table, which will fail if the layout is not the expected
one.

To reduce the risk for confusion, the recommendation is to use a specific
database for tables that contain documents.

# Database Commands

The following lists all implemented MongoDB commands and to what extent
they are supported. Each heading links to the corresponding entry in the
MongoDB documentation.

## [Aggregation Commands](https://docs.mongodb.com/manual/reference/command/nav-aggregation/)

### [count](https://docs.mongodb.com/manual/reference/command/count/)

TBW

### [distinct](https://docs.mongodb.com/manual/reference/command/distinct/)

TBW

## [Geospatial Commands](https://docs.mongodb.com/manual/reference/command/nav-geospatial/)

No commands from this group are currently supported.

## [Query and Write Operation Commands](https://docs.mongodb.com/manual/reference/command/nav-crud/)

### [delete](https://docs.mongodb.com/manual/reference/command/insert)

The following fields are acted upon.

Field | Type | Description
------|------|------------
insert| string | The name of the target table.
deletes | array | An array of one document that describes what to delete.

All other fields are ignored.

**NOTE** Currently the `deletes` array can contain exactly one document.

### [find](https://docs.mongodb.com/manual/reference/command/find)

The following fields are acted upon.

Field | Type | Description
------|------|------------
find| string | The name of the target table.
projection | document | Optional. The projection specification to determine which fields to includein the returned documents.

All other fields are ignored.

#### Projection

The `projection` parameter determines which fields are returned in the matching documents.
The `projection` parameter takes a document of the following form:
```
{ <field1>: <value>, <field2>: <value> ... }
```

If a `projection` document is not provided or if it is empty, the entire document
will be returned.

Projection | Description
-----------|------------
`<field>: <1 or true>` | Specifies the inclusion of a field.
`<field>: <0 or false>` | Specifies the exclusion of a field.

##### Embedded Field Specification
For fields in an embedded documents, the field can be specified using:

   * _dot notation_; e.g. `"field.nestedfield": <value>`

Specifying fields in embedded documents using nested form is not supported.

##### `_id` Field Projection
The `_id` field is included in the returned documents by default unless you
explicitly specify `_id: 0` in the projection to suppress the field.

#### Inclusion or Exclusion
A `projection` cannot contain both include and exclude specifications,
with the exception of the `_id` field:

   * In projections that _explicitly_ include fields, the `_id` field is the only field that can be explicitly excluded.
   * In projections that _explicitly_ excludes fields, the `_id` field is the only field that can be explicitly include; however, the `_id` field is included by default.

*NOTE* Currently `_id` is the only field that can be excluded, and _only_
if other fields are explicitly included.
*NOTE* Currently exclusion of other fields but `_id` is currently not supported.

### [getLastError](https://docs.mongodb.com/manual/reference/command/getLastError/)

TBI

### [getMore](https://docs.mongodb.com/manual/reference/command/getMore/)

TBW

### [insert](https://docs.mongodb.com/manual/reference/command/insert)

The `insert` command inserts one or more documents into the table whose
name is the same as that of the collection. If the option `auto_create_tables`
is `true`, then the table is created if it does not already exist. If the
value is `false`, then the insert will fail unless the table already exists.

The following fields are acted upon.

Field | Type | Description
------|------|------------
insert| string | The name of the target collection/table.
documents | array | An array of one or more documents to be inserted to the named collection/table.
ordered | boolean | Optional, with default being `true`. See below for description.

All other fields are ignored.

#### `ordered`
The impact of `ordered` is dependent upon the value of `insert_behavior'.

##### `as_mongodb`
In this case `ordered` has the same impact as in MongoDB. That is, if the value
is `true`, then when an insert of a document fails, return without inserting any
remaining documents listed in the inserts array. If `false`, then when an insert
of a document fails, continue to insert the remaining documents.

##### `as_mariadb`
If `ordered` is `true`, then all documents will be inserted using a single
INSERT command. That is, if the insertion of any document fails, for instance,
due to a duplicate id, then no document will be inserted. If `ordered` is `false`,
then the behavior is identical with that of `as_mongodb`.

### [resetError](https://docs.mongodb.com/manual/reference/command/getLastError/)

TBI

### [update](https://docs.mongodb.com/manual/reference/command/update)

The following fields are acted upon.

Field | Type | Description
------|------|------------
update | string | The name of the target table.
updates | array | An array of documents that describe what to updated.

All other fields are ignored.

**NOTE** Currently the `updates` array can contain exactly one document.

#### Update Statements

Each element of the updates array is an update statement document.
Each document contains the following fields:

Field | Type | Description
------|------|------------
q | document | The query that matches documents to update.
u | document | The documents to apply. Currently _only_ a replacement document with only `<field1>: <value1>` pairs are supported.
multi| boolean | Optional. If `true`, updates all documents that meet the query criteria. If `false liit the update to one document that meets the query criteria. Defaults to `false`.

All other fields are ignored.

## [Query Plan Cache Commands](https://docs.mongodb.com/manual/reference/command/nav-plan-cache/)

No commands from this group are currently supported.

## [Authenitcation Commands](https://docs.mongodb.com/manual/reference/command/nav-authentication/)

No commands from this group are currently supported.

## [User Management Commands](https://docs.mongodb.com/manual/reference/command/nav-user-management/)

No commands from this group are currently supported.

## [Role Management Commands](https://docs.mongodb.com/manual/reference/command/nav-role-management/)

No commands from this group are currently supported.

## [Replication Commands](https://docs.mongodb.com/manual/reference/command/nav-replication/)

### [isMaster](https://docs.mongodb.com/manual/reference/command/isMaster/)

TBW

## [Sharding Commands](https://docs.mongodb.com/manual/reference/command/nav-sharding/)

No commands from this group are currently supported.

## [Sessions Commands](https://docs.mongodb.com/manual/reference/command/nav-sessions/)

### [endSessions](https://docs.mongodb.com/manual/reference/command/endSessions/)

TBW

## [Administration Commands](https://docs.mongodb.com/manual/reference/command/nav-administration/)

### [create](https://docs.mongodb.com/manual/reference/command/create/)

TBW

### [drop](https://docs.mongodb.com/manual/reference/command/drop/)

TBW

### [dropDatabase](https://docs.mongodb.com/manual/reference/command/dropDatabase/)

TBW

### [killCursors](https://docs.mongodb.com/manual/reference/command/killCursors/)

TBW

### [listCollections](https://docs.mongodb.com/manual/reference/command/listCollections/)

TBW

### [listDatabases](https://docs.mongodb.com/manual/reference/command/listDatabases/)

TBW

### [renameCollection](https://docs.mongodb.com/manual/reference/command/renameCollection/)

TBW

## [Diagnostic Commands](https://docs.mongodb.com/manual/reference/command/nav-diagnostic/)

### [buildInfo](https://docs.mongodb.com/manual/reference/command/buildInfo/)

TBW

### [getCmdLineOpts](https://docs.mongodb.com/manual/reference/command/getCmdLineOpts/)

TBW

### [getLog](https://docs.mongodb.com/manual/reference/command/getLog/)

TBW

### [listCommands](https://docs.mongodb.com/manual/reference/command/listCommands/)

TBW

### [ping](https://docs.mongodb.com/manual/reference/command/ping/)

TBW

### [whatsmyuri](https://docs.mongodb.com/manual/reference/command/whatsmyuri/)

TBW

## [Free Monitoring Commands](https://docs.mongodb.com/manual/reference/command/nav-free-monitoring/)

### [getFreeMonitoringStatus](https://docs.mongodb.com/manual/reference/command/getFreeMonitoringStatus/)

TBW

## [System Events Auditing Commands](https://docs.mongodb.com/manual/reference/command/nav-auditing/)

No commands from this group are currently supported.

## MaxScale Specific Commands

### mxsDiagnose

#### Definition

##### **mxsDiagnose**

The `mxsDiagnose` command provides diagnostics for any other command; that is, how
MaxScale will handle that command.

#### Syntax

The `mxsDiagnose` command has the following syntaxt:
```
db.runCommand(
    {
       mxsDiagnose: <command>
    }
)
```
##### Command Fields

The command takes the following fields:

Field | Type | Description
------|------|------------
mxsDiagnose | document | A command as provided to `db.runCommand(...)`.

##### Returns

The command returns a document that contains diagnostics of the command
provided as argument. For example:
```
> db.runCommand({mxsDiagnose: {ping:1}});
{ "kind" : "immediate", "response" : { "ok" : 1 }, "ok" : 1 }

> db.runCommand({mxsDiagnose: {find:"person", filter: { name: "Bob"}}});
{
  "kind" : "single",
  "sql" : "SELECT doc FROM `test`.`person` WHERE ( JSON_EXTRACT(doc, '$.name') = 'Bob') ",
  "ok" : 1
}

> db.runCommand({mxsDiagnose: {delete:"person", deletes: [{q: { name: "Bob"}, limit:0}, {q: {name: "Alice"}, limit:0}]}});
{
  "kind" : "single",
  "sql" : [
    "DELETE FROM `test`.`person` WHERE ( JSON_EXTRACT(doc, '$.name') = 'Bob') ",
    "DELETE FROM `test`.`person` WHERE ( JSON_EXTRACT(doc, '$.name') = 'Alice') "
  ],
  "ok" : 1
}
```
`kind` specifies of what kind the command is; an _immediate_ command is one for
which MaxScale autonomously can generate the response, a _single_ command is one
where the command will cause a single SQL statement to be sent to the backend, and
a _multi_ command is one where potentially multiple SQL statements will be sent to
the backend.

If the command is _immediate_ then there will be a field `response` containing
the actual response of the command, if the command is _single_ then there will be
a field `sql` containing the actual statement that would have been sent to the backend,
and if the command is _multi_ then there will be a field `sql` containing an array
of statements that would have been sent to the backend.

If an error occurs while the command is being diagnosed, then `ok` will be `0`
and there will be a field `error` whose value is an error object.

### mxsGetConfig

#### Definition

#### **mxsGetConfig**

The `mxsGetConfig` command returns the current configuration of the session.

#### Syntax

The `mxsGetConfig` has the following syntax:
```
db.runCommand(
    {
        mxsGetConfig: <any>
    });
```
##### Command Fields

The command takes the following fields:

Field | Type | Description
------|------|------------
mxsGetConfig | <any> | Ignored.

##### Returns

The command returns a document that contains the current configuration of
the session. For example:
```
> db.runCommand({mxsGetConfig: 1});
{
	"config" : {
		"on_unknown_command" : "return_error",
		"auto_create_tables" : true,
		"id_length" : 24
	},
	"ok" : 1
}
```

### mxsSetConfig

#### Definition

#### **mxsSetConfig**

The `mxsSetConfig` command changes the configuration of the session.

Note that the changes only affect the current session and are **not**
persisted.

#### Syntax

The `mxsSetConfig` has the following syntax:
```
db.runCommand(
    {
        mxsSetConfig: document
    });
```
##### Command Fields

The command takes the following fields:

Field | Type | Description
------|------|------------
mxsSetConfig | document | A document specifying the configuration.

The document takes the following fields:

Field | Type | Description
------|------|------------
on_unknown_command | string | Either `"return_error"` or `"return_empty"`
auto_create_tables | boolean | Whether tables should be created as needed.
id_length | integer | `id` column `VARCHAR` size in created tables.

##### Returns

The command returns a document that contains the changed configuration of
the session. For example:
```
> db.runCommand({mxsGetConfig: 1});
{
	"config" : {
		"on_unknown_command" : "return_error",
		"auto_create_tables" : true,
		"id_length" : 24
	},
	"ok" : 1
}
> db.runCommand({mxsSetConfig: { auto_create_tables: false}});
{
	"config" : {
		"on_unknown_command" : "return_error",
		"auto_create_tables" : false,
		"id_length" : 24
	},
	"ok" : 1
}

```

# Object Id

When a document is created, an id of type `ObjectId` will be autogenerated by
the MongoDB client library. If the id is provided explicitly, by assigning a
value to the `_id` field, the value must be an `ObjectId`, a string or an
integer.
