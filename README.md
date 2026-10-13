# Equipment Lending Library

This repository currently contains the first EC2 deployment checkpoint: a small
C HTTP landing page with a health endpoint. The SQLite database is initialized
by `start.sh`; the lending-library schema and Mongoose integration will be
added in later checkpoints.

## Local prerequisites

On Ubuntu/Debian, install the tools before running the app:

```bash
sudo apt-get update
sudo apt-get install -y gcc sqlite3
```

Start locally:

```bash
./start.sh
```

Open <http://localhost:8080/>. The health check is at
<http://localhost:8080/api/health>. Set `PORT` to use another port.

## AWS EC2 deployment

The VM in this assignment is an **AWS EC2 instance**. Use an Ubuntu EC2 image,
keep the downloaded `.pem` file outside this repository (for example in
`~/keys/`), and use the `ubuntu` SSH user. The instance security group must
allow inbound TCP port 22 from your IP and TCP port 80 from the internet.

From the repository, copy and run the one-time setup on the server:

```bash
scp -i ~/keys/mykey.pem -r deploy ubuntu@PUBLIC_IP:~
ssh -i ~/keys/mykey.pem ubuntu@PUBLIC_IP
sudo bash deploy/setup-ec2.sh
```

From your laptop, deploy later changes with:

```bash
./deploy/deploy.sh -h PUBLIC_IP -i ~/keys/mykey.pem
```

The service runs as the `ubuntu` user, is managed by systemd, and is proxied
through nginx on port 80. To verify startup after a reboot:

```bash
sudo reboot
```

Wait for the instance to return, then open `http://PUBLIC_IP/` again.

Never commit `.pem` keys or `.env` files. They are ignored by this repository.
