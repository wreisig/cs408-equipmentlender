# Equipment Lending Library

Checkpoint 1 provides a reproducible C/Mongoose/SQLite application foundation.
The Mongoose server currently serves the Home page and `/api/health`; the
equipment workflows will be implemented in later checkpoints.

## Technology and testing

- Backend: C with the Mongoose Web Server
- Database: SQLite
- Frontend: HTML with Bootstrap 5
- Unit-test framework: Unity C unit-testing framework

Mongoose is downloaded at a pinned version (`7.17`) by `start.sh` and
`deploy/deploy.sh`, so a fresh clone does not require a separately installed
Mongoose package.

## Local installation and run

On Ubuntu/Debian, install the system tools once:

```bash
sudo apt-get update
sudo apt-get install -y gcc sqlite3 libsqlite3-dev curl
```

From the repository root on your laptop:

```bash
./start.sh
```

The script downloads Mongoose if needed, compiles the C server, initializes the
SQLite schema in `data/equipment.db`, and starts the app. Open
<http://localhost:8080/>. The health check is
<http://localhost:8080/api/health>. Use `PORT=8081 ./start.sh` to select a
different port. Stop the server with `Ctrl+C`.

## EC2 setup and deployment

The VM is an **AWS EC2 Ubuntu instance**. Keep the `.pem` key outside this
repository, use the `ubuntu` login, and allow inbound TCP 22 from your IP and
TCP 80 from the internet.

### One-time setup on the EC2 server

Run these first two commands on your laptop from the repository root:

```bash
scp -i ~/keys/mykey.pem -r deploy ubuntu@PUBLIC_IP:~
ssh -i ~/keys/mykey.pem ubuntu@PUBLIC_IP
```

Then run this command inside the EC2 terminal:

```bash
sudo bash deploy/setup-ec2.sh
```

This installs GCC, SQLite development libraries, curl, nginx, and rsync. It
creates the application directories, installs the systemd unit, configures
nginx to forward port 80 to the app on port 8080, and enables the service at
boot.

### Repeatable deployment from the laptop

Run this from the repository root on your laptop:

```bash
./deploy/deploy.sh -h PUBLIC_IP -i ~/keys/mykey.pem
```

The deployment script copies the source, downloads the pinned Mongoose
dependency on EC2, compiles the server, restarts systemd, and checks
`/api/health`.

Open the live site at `http://PUBLIC_IP/`. To verify startup on boot, run this
on EC2:

```bash
sudo reboot
```

After the instance returns, reload the live URL without logging in again.

Never commit `.pem` keys or `.env` files. They are ignored by this repository.

## Checkpoint 1 scope

### Core functionality

- Dashboard and equipment statistics
- Equipment catalog and details
- Search and filtering
- Equipment CRUD
- Loans and returns
- Borrowing history
- Input validation and success/error messages
- Maintenance records
- SQLite persistence
- EC2 deployment
- Automated tests

### Stretch functionality

- Authentication and role-based access
- Equipment image uploads
- Email reminders
- Reports and data exports
- Reservation queues

Checkpoint 1 delivers the Mongoose Home page, health endpoint, repeatable
startup, EC2 deployment, and documented run instructions. Checkpoint 2
formalizes the SQLite persistence foundation and initial tables. Later
checkpoints implement the remaining core functionality.

## Automated testing

The project will use the Unity C unit-testing framework. The test suite will
cover database initialization and operations, equipment creation and updates,
search and filtering, input validation, loan creation, equipment returns,
maintenance records, and rules preventing unavailable equipment from being
borrowed. Tests will run locally and again against the deployed EC2
application.
