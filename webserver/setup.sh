#!/bin/bash
# Setup script for lighttpd remote admin server on Linux
# This script sets up a complete remote admin web server with CGI support

set -e  # Exit on any error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WEB_ROOT="/var/www/remote-admin"
VENV_DIR="$WEB_ROOT/venv"

echo "=========================================="
echo "   Remote Admin Web Server Setup Script"
echo "=========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

check_root() {
    if [[ $EUID -ne 0 ]]; then
        echo -e "${RED}Error: This script must be run as root!${NC}"
        echo "Run: sudo ./setup.sh"
        exit 1
    fi
}

log_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

log_error() {
    echo -e "${RED}✗ $1${NC}"
}

log_info() {
    echo -e "${YELLOW}» $1${NC}"
}

check_root

# Create directories first
echo ""
log_info "Creating directories..."
mkdir -p "$WEB_ROOT"
mkdir -p /var/log/lighttpd
chmod 750 /var/log/lighttpd
log_success "Directories created"

# Update and install packages
echo ""
log_info "Installing packages..."
apt update -qq
apt install -y lighttpd python3 python3-pip python3-venv postgresql postgresql-contrib ssh > /dev/null 2>&1
log_success "Packages installed"

# Create virtual environment
echo ""
log_info "Creating Python virtual environment..."
python3 -m venv "$VENV_DIR"
log_success "Virtual environment created"

# Upgrade pip in venv
echo ""
log_info "Installing Python dependencies..."
"$VENV_DIR/bin/pip" install --upgrade pip > /dev/null 2>&1
"$VENV_DIR/bin/pip" install psycopg2-binary > /dev/null 2>&1
log_success "Python dependencies installed"

# Copy CGI files
echo ""
log_info "Copying CGI files..."
CGI_FOUND=0
for cgi_file in "$SCRIPT_DIR"/*.cgi; do
    if [ -f "$cgi_file" ]; then
        CGI_FOUND=1
        base_name="$(basename "$cgi_file")"
        cp "$cgi_file" "$WEB_ROOT/$base_name"
        chmod +x "$WEB_ROOT/$base_name"
        log_success "Copied $base_name"
    fi
done

if [ "$CGI_FOUND" -eq 0 ]; then
    log_error "No CGI files (*.cgi) found in $SCRIPT_DIR"
fi

# Copy Python helper files
echo ""
log_info "Copying Python helper files..."
PY_FOUND=0
for py_file in "$SCRIPT_DIR"/*.py; do
    if [ -f "$py_file" ]; then
        PY_FOUND=1
        base_name="$(basename "$py_file")"
        cp "$py_file" "$WEB_ROOT/$base_name"
        chmod 644 "$WEB_ROOT/$base_name"
        log_success "Copied $base_name"
    fi
done

if [ "$PY_FOUND" -eq 0 ]; then
    log_info "No Python helper files (*.py) found in $SCRIPT_DIR"
fi

# Copy HTML files
echo ""
log_info "Copying HTML/static files..."
HTML_FOUND=0
for html_file in "$SCRIPT_DIR"/*.html; do
    if [ -f "$html_file" ]; then
        HTML_FOUND=1
        base_name="$(basename "$html_file")"
        cp "$html_file" "$WEB_ROOT/$base_name"
        log_success "Copied $base_name"
    fi
done

if [ "$HTML_FOUND" -eq 0 ]; then
    log_info "No HTML files (*.html) found in $SCRIPT_DIR"
fi

# Copy CSS files
echo ""
log_info "Copying CSS files..."
CSS_FOUND=0
for css_file in "$SCRIPT_DIR"/*.css; do
    if [ -f "$css_file" ]; then
        CSS_FOUND=1
        base_name="$(basename "$css_file")"
        cp "$css_file" "$WEB_ROOT/$base_name"
        log_success "Copied $base_name"
    fi
done

if [ "$CSS_FOUND" -eq 0 ]; then
    log_info "No CSS files (*.css) found in $SCRIPT_DIR"
fi

# Set permissions
echo ""
log_info "Setting file permissions..."
chown -R www-data:www-data "$WEB_ROOT"
chown -R www-data:www-data /var/log/lighttpd
touch /var/log/lighttpd/error.log
touch /var/log/lighttpd/access.log
chmod 644 /var/log/lighttpd/error.log
chmod 644 /var/log/lighttpd/access.log
log_success "Permissions configured"

# Copy lighttpd configuration
echo ""
log_info "Configuring lighttpd..."
if [ -f "$SCRIPT_DIR/lighttpd.conf" ]; then
    cp "$SCRIPT_DIR/lighttpd.conf" /etc/lighttpd/lighttpd.conf
    log_success "Configuration copied"
else
    log_error "lighttpd.conf not found in $SCRIPT_DIR"
    exit 1
fi

# Test configuration
echo ""
log_info "Testing lighttpd configuration..."
if lighttpd -t -f /etc/lighttpd/lighttpd.conf > /dev/null 2>&1; then
    log_success "Configuration is valid"
else
    log_error "Configuration test failed!"
    lighttpd -t -f /etc/lighttpd/lighttpd.conf
    exit 1
fi

# SSH setup
echo ""
echo "=========================================="
echo "   SSH Setup"
echo "=========================================="
echo ""

SSH_ACCESS_USER="${SSH_ACCESS_USER:-remote_admin_ssh}"
SSH_ACCESS_HOME="/home/$SSH_ACCESS_USER"
NOLOGIN_SHELL="$(command -v nologin || echo /usr/sbin/nologin)"

log_info "Ensuring SSH access user exists: $SSH_ACCESS_USER"
if id "$SSH_ACCESS_USER" >/dev/null 2>&1; then
    log_info "User '$SSH_ACCESS_USER' already exists"
    usermod -s "$NOLOGIN_SHELL" "$SSH_ACCESS_USER"
    passwd -l "$SSH_ACCESS_USER" > /dev/null 2>&1 || true
    log_success "Updated '$SSH_ACCESS_USER' to non-login shell"
else
    useradd -m -s "$NOLOGIN_SHELL" "$SSH_ACCESS_USER"
    passwd -l "$SSH_ACCESS_USER" > /dev/null 2>&1 || true
    log_success "Created user '$SSH_ACCESS_USER'"
fi

log_info "Backing up sshd_config..."
sudo cp /etc/ssh/sshd_config /etc/ssh/sshd_config.bak

log_info " Applying secure SSH configuration..."

# Remove existing relevant settings to avoid duplicates
sudo sed -i '/^AllowTcpForwarding/d' /etc/ssh/sshd_config
sudo sed -i '/^PermitOpen/d' /etc/ssh/sshd_config
sudo sed -i '/^GatewayPorts/d' /etc/ssh/sshd_config
sudo sed -i '/^AllowUsers/d' /etc/ssh/sshd_config

# Append secure configuration
sudo tee -a /etc/ssh/sshd_config > /dev/null <<EOF

PubkeyAuthentication yes
PasswordAuthentication no
#KbdInteractiveAuthentication no
#ChallengeResponseAuthentication no
AuthorizedKeysFile .ssh/authorized_keys
AllowUsers $SSH_ACCESS_USER
AllowTcpForwarding remote
PermitTTY no
GatewayPorts no
AllowAgentForwarding no
AllowStreamLocalForwarding no
X11Forwarding no

EOF

log_info "Restarting SSH service..."

if command -v systemctl >/dev/null 2>&1; then
    sudo systemctl restart ssh || sudo systemctl restart sshd
    sudo systemctl enable ssh || sudo systemctl enable sshd
else
    sudo service ssh restart || sudo service sshd restart
fi

log_info "SSH is now configured to allow ONLY remote port forwarding (no interactive shells or other access)."

log_info "Generating key pairs.."
if ssh-keygen -t ed25519 -a 100 -C "$SSH_ACCESS_USER@$(hostname)" -f "$SCRIPT_DIR/key" -N ""; then
    log_success "Key pair generated"
else
    log_error "Key pair generation failed"
    exit 1
fi

mkdir -p "$SSH_ACCESS_HOME/.ssh"
chmod 700 "$SSH_ACCESS_HOME/.ssh"
cat "$SCRIPT_DIR/key.pub" >> "$SSH_ACCESS_HOME/.ssh/authorized_keys"
chmod 600 "$SSH_ACCESS_HOME/.ssh/authorized_keys"
chown -R "$SSH_ACCESS_USER:$SSH_ACCESS_USER" "$SSH_ACCESS_HOME/.ssh"

log_success "Public key added to $SSH_ACCESS_USER authorized_keys"
# Database setup
echo ""
echo "=========================================="
echo "   PostgreSQL Database Setup"
echo "=========================================="
echo ""

SCHEMA_FILE=""
for candidate in "$SCRIPT_DIR"/*schema.sql "$SCRIPT_DIR"/*.sql; do
    if [ -f "$candidate" ]; then
        SCHEMA_FILE="$candidate"
        break
    fi
done

if [ -n "$SCHEMA_FILE" ]; then
    log_info "Found schema file: $(basename "$SCHEMA_FILE")"
    
    # Hardcoded database identifiers (no prompt)
    DB_NAME="remote_admin_db"
    DB_USER="remote_admin_db_user"

    # Password must be provided by env var or interactive prompt
    if [ -n "$DB_PASS" ]; then
        log_info "Using database password from DB_PASS environment variable"
    else
        read -sp "Database password for '$DB_USER': " DB_PASS
        echo ""
    fi

    if [ -z "$DB_PASS" ]; then
        log_error "Database password cannot be empty"
        exit 1
    fi

    # Create DB config file for CGI scripts
    DB_CONFIG_FILE="$WEB_ROOT/db.conf"
    log_info "Writing database config file..."
    {
        printf "[database]\n"
        printf "name=%s\n" "$DB_NAME"
        printf "user=%s\n" "$DB_USER"
        printf "password=%s\n" "$DB_PASS"
    } > "$DB_CONFIG_FILE"
    chown root:www-data "$DB_CONFIG_FILE"
    chmod 640 "$DB_CONFIG_FILE"
    log_success "Database config written to $DB_CONFIG_FILE"
    
    # Ensure PostgreSQL is running
    log_info "Starting PostgreSQL service..."
    systemctl start postgresql
    systemctl enable postgresql > /dev/null 2>&1
    log_success "PostgreSQL started"
    
    # Create database and user
    echo ""
    log_info "Creating database and user..."
    
    # Check if database exists
    if sudo -u postgres psql -lqt | cut -d \| -f 1 | grep -qw "$DB_NAME"; then
        log_info "Database '$DB_NAME' already exists"
    else
        sudo -u postgres psql -c "CREATE DATABASE $DB_NAME;" > /dev/null 2>&1
        log_success "Database '$DB_NAME' created"
    fi
    
    # Check if user exists
    if sudo -u postgres psql -tAc "SELECT 1 FROM pg_roles WHERE rolname='$DB_USER'" | grep -q 1; then
        log_info "User '$DB_USER' already exists"
        sudo -u postgres psql -c "ALTER USER $DB_USER WITH PASSWORD '$DB_PASS';" > /dev/null 2>&1
        log_success "Password updated for '$DB_USER'"
    else
        sudo -u postgres psql -c "CREATE USER $DB_USER WITH PASSWORD '$DB_PASS';" > /dev/null 2>&1
        log_success "User '$DB_USER' created"
    fi
    
    # Grant privileges
    sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE $DB_NAME TO $DB_USER;" > /dev/null 2>&1
    sudo -u postgres psql -d "$DB_NAME" -c "GRANT ALL PRIVILEGES ON ALL TABLES IN SCHEMA public TO $DB_USER;" > /dev/null 2>&1
    sudo -u postgres psql -d "$DB_NAME" -c "GRANT ALL PRIVILEGES ON ALL SEQUENCES IN SCHEMA public TO $DB_USER;" > /dev/null 2>&1
    sudo -u postgres psql -d "$DB_NAME" -c "ALTER DEFAULT PRIVILEGES IN SCHEMA public GRANT ALL ON TABLES TO $DB_USER;" > /dev/null 2>&1
    sudo -u postgres psql -d "$DB_NAME" -c "ALTER DEFAULT PRIVILEGES IN SCHEMA public GRANT ALL ON SEQUENCES TO $DB_USER;" > /dev/null 2>&1
    log_success "Privileges granted"
    
    # Import schema
    echo ""
    log_info "Importing database schema..."
    # Feed schema through stdin so postgres does not need traverse permissions on SCRIPT_DIR.
    if sudo -u postgres psql -v ON_ERROR_STOP=1 -d "$DB_NAME" > /dev/null 2>&1 < "$SCHEMA_FILE"; then
        log_success "Schema imported with postgres user"
    else
        log_error "Schema import failed with postgres user"
        log_info "You may need to import manually: sudo -u postgres psql -d $DB_NAME < $SCHEMA_FILE"
    fi
    
    # Test connection
    echo ""
    log_info "Testing database connection..."
    if PGPASSWORD="$DB_PASS" psql -v ON_ERROR_STOP=1 -h localhost -U "$DB_USER" -d "$DB_NAME" -c "SELECT 1;" > /dev/null 2>&1; then
        log_success "Database connection successful"
    else
        log_error "Database connection failed!"
        log_info "Please verify credentials and try connecting manually"
    fi
    
    # Store credentials for reference
    echo ""
    log_info "Database credentials:"
    echo "  Database: $DB_NAME"
    echo "  User: $DB_USER"
    echo "  Password: (provided at runtime)"
    echo "  Host: localhost"
    echo ""
else
    log_info "No schema SQL file found - skipping database setup"
    log_info "You can import the schema manually later"
fi

# Start lighttpd
echo ""
log_info "Starting lighttpd service..."
systemctl restart lighttpd
systemctl enable lighttpd
log_success "Lighttpd started and enabled"

# Generate another SSH key pair for local server use
log_info "Generating local server SSH key pair (local_server_key)..."
if ssh-keygen -t ed25519 -a 100 -C "local_server_key@$(hostname)" -f "$SCRIPT_DIR/local_server_key" -N ""; then
    log_success "Local server key pair generated: $SCRIPT_DIR/local_server_key and local_server_key.pub"
else
    log_error "Local server key pair generation failed"
    exit 1
fi

# Copy local_server_key and local_server_key.pub to ../ssh_keys
SSH_KEYS_DIR="$SCRIPT_DIR/../ssh_keys"
mkdir -p "$SSH_KEYS_DIR"
chmod 700 "$SSH_KEYS_DIR"
cp "$SCRIPT_DIR/local_server_key" "$SSH_KEYS_DIR/"
cp "$SCRIPT_DIR/local_server_key.pub" "$SSH_KEYS_DIR/"
cp "$SCRIPT_DIR/key" "$SSH_KEYS_DIR/"
chmod 600 "$SSH_KEYS_DIR/local_server_key"
chmod 644 "$SSH_KEYS_DIR/local_server_key.pub"
log_success "Copied local_server_key and local_server_key.pub to $SSH_KEYS_DIR"

# Generate a dedicated host_key pair in webserver dir
log_info "Generating host key pair (host_key)..."
if ssh-keygen -t ed25519 -a 100 -C "host_key@$(hostname)" -f "$SCRIPT_DIR/host_key" -N ""; then
    log_success "Generated host_key and host_key.pub"
else
    log_error "host_key pair generation failed"
    exit 1
fi

# Add host_key.pub to the invoking user's known_hosts
CURRENT_USER="${SUDO_USER:-$USER}"
CURRENT_USER_HOME="$(getent passwd "$CURRENT_USER" | cut -d: -f6)"
if [ -z "$CURRENT_USER_HOME" ]; then
    log_error "Could not determine home directory for user '$CURRENT_USER'"
    exit 1
fi

KNOWN_HOSTS_DIR="$CURRENT_USER_HOME/.ssh"
KNOWN_HOSTS_FILE="$KNOWN_HOSTS_DIR/known_hosts"
mkdir -p "$KNOWN_HOSTS_DIR"
touch "$KNOWN_HOSTS_FILE"

HOSTS_LIST="localhost,127.0.0.1,$(hostname)"
SERVER_IP_FOR_KNOWN_HOSTS="$(hostname -I 2>/dev/null | awk '{print $1}')"
if [ -n "$SERVER_IP_FOR_KNOWN_HOSTS" ]; then
    HOSTS_LIST="$HOSTS_LIST,$SERVER_IP_FOR_KNOWN_HOSTS"
fi

PUBKEY_TYPE_AND_DATA="$(awk '{print $1" "$2}' "$SCRIPT_DIR/host_key.pub")"
KNOWN_HOSTS_ENTRY="$HOSTS_LIST $PUBKEY_TYPE_AND_DATA"

if ! grep -Fqx "$KNOWN_HOSTS_ENTRY" "$KNOWN_HOSTS_FILE"; then
    echo "$KNOWN_HOSTS_ENTRY" >> "$KNOWN_HOSTS_FILE"
    log_success "Added host_key.pub to $KNOWN_HOSTS_FILE"
else
    log_info "host_key entry already exists in $KNOWN_HOSTS_FILE"
fi

chmod 700 "$KNOWN_HOSTS_DIR"
chmod 600 "$KNOWN_HOSTS_FILE"
chown "$CURRENT_USER:$CURRENT_USER" "$KNOWN_HOSTS_DIR" "$KNOWN_HOSTS_FILE"

# Ensure ssh_keys and local_server_key are usable by current user with ssh -i
chown "$CURRENT_USER:$CURRENT_USER" "$SSH_KEYS_DIR"
chown "$CURRENT_USER:$CURRENT_USER" "$SSH_KEYS_DIR/local_server_key" "$SSH_KEYS_DIR/local_server_key.pub"
chmod 700 "$SSH_KEYS_DIR"
chmod 600 "$SSH_KEYS_DIR/local_server_key"
chmod 644 "$SSH_KEYS_DIR/local_server_key.pub"

# Move host_key pair to ssh_keys and keep it out of webserver dir
mv "$SCRIPT_DIR/host_key" "$SSH_KEYS_DIR/host_key"
mv "$SCRIPT_DIR/host_key.pub" "$SSH_KEYS_DIR/host_key.pub"
chmod 600 "$SSH_KEYS_DIR/host_key"
chmod 644 "$SSH_KEYS_DIR/host_key.pub"
log_success "Moved host_key and host_key.pub to $SSH_KEYS_DIR"

# Remove key pairs from webserver directory
rm -f "$SCRIPT_DIR/key" "$SCRIPT_DIR/key.pub" "$SCRIPT_DIR/local_server_key" "$SCRIPT_DIR/local_server_key.pub" "$SCRIPT_DIR/host_key" "$SCRIPT_DIR/host_key.pub"
log_success "Removed key, key.pub, local_server_key, local_server_key.pub, host_key, and host_key.pub from $SCRIPT_DIR"



# Final status
echo ""
echo "=========================================="
echo -e "${GREEN}Setup completed successfully!${NC}"
echo "=========================================="
echo ""

SERVER_IP=$(hostname -I | awk '{print $1}')
echo "Server Information:"
echo "  URL: http://$SERVER_IP:8080/index.html"
echo "  API: http://$SERVER_IP:8080/getinfos.cgi"
echo ""
echo "Configuration:"
echo "  Web Root: $WEB_ROOT"
echo "  Venv: $VENV_DIR"
echo "  Logs: /var/log/lighttpd/"
echo ""
echo "Useful commands:"
echo "  View errors:   sudo tail -f /var/log/lighttpd/error.log"
echo "  View access:   sudo tail -f /var/log/lighttpd/access.log"
echo "  Restart:       sudo systemctl restart lighttpd"
echo "  Status:        sudo systemctl status lighttpd"
echo ""
echo "Next steps:"
echo "  1. CGI DB config file: $WEB_ROOT/db.conf"
echo "  2. Test connection: curl http://localhost:8080/getinfos.cgi"
echo "  3. Deploy remote access clients"
echo ""
