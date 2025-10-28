#!/bin/bash
# build_and_deploy.sh - Xregulator OTA Build and Deploy Script with Version Notes
# Usage: ./build_and_deploy.sh 2.1.0 "Fixed temperature sensor bug, added new dashboard features"

set -e  # Exit on any error

# Configuration
PROJECT_PATH="/Users/joeceo/Documents/Arduino/Xregulator"
FQBN="esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=custom,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc"
PRIVATE_KEY="/Users/joeceo/private_key.pem"
SERVER_USER="root"
SERVER_HOST="host.xengineering.net"
SERVER_PATH="/home/ota/htdocs/ota.xengineering.net"
SERVER_PASS="a47H8IE0vgeEe5efgwe"

# Check arguments
if [ -z "$1" ]; then
    echo "Usage: $0 <version> [notes]"
    echo "   Example: $0 2.1.0 \"Fixed temperature sensor bug, added new dashboard features\""
    exit 1
fi

VERSION="$1"
NOTES="${2:-Version $VERSION}"

# Validate description length
NOTES_LENGTH=${#NOTES}
if [ $NOTES_LENGTH -gt 200 ]; then
    echo "❌ ERROR: Version description too long ($NOTES_LENGTH characters)"
    echo "   Maximum recommended: 200 characters"
    echo "   Your description: \"$NOTES\""
    echo ""
    echo "   Suggestion: Keep it brief - users want quick summaries, not essays"
    echo ""
    read -p "   Continue anyway? (y/n): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Deployment cancelled"
        exit 1
    fi
fi

echo "🚀 === XREGULATOR OTA BUILD & DEPLOY v$VERSION ==="
echo "📝 Notes: $NOTES"

# [Your existing prerequisite checks - keeping original logic]
echo "🔍 Checking prerequisites..."

if ! command -v arduino-cli &> /dev/null; then
    echo "❌ Arduino CLI not found. Install with: brew install arduino-cli"
    exit 1
fi

if [ ! -f "$PRIVATE_KEY" ]; then
    echo "❌ Private key not found at: $PRIVATE_KEY"
    exit 1
fi

if [ ! -d "$PROJECT_PATH" ]; then
    echo "❌ Project path not found: $PROJECT_PATH"
    exit 1
fi

if [ ! -d "$PROJECT_PATH/data" ]; then
    echo "❌ Web files folder not found: $PROJECT_PATH/data"
    exit 1
fi

if ! command -v sshpass &> /dev/null; then
    echo "📦 Installing sshpass for automated uploads..."
    brew install sshpass
fi

echo "✅ All prerequisites met"

# Navigate to project
cd "$PROJECT_PATH"

# Clean previous builds and clear caches
echo "🧹 Cleaning previous builds and clearing caches..."
rm -rf build/ 2>/dev/null || true
rm -rf temp_package/ 2>/dev/null || true
rm -f firmware_*.tar 2>/dev/null || true
rm -f firmware_*.sig.* 2>/dev/null || true

# Clear Arduino CLI cache
echo "🗑️ Clearing Arduino CLI cache..."
arduino-cli cache clean

# Clear Arduino IDE cache (if it exists)
if [ -d "$HOME/Library/Arduino15/packages/esp32/hardware/esp32/cache" ]; then
    echo "🗑️ Clearing Arduino IDE cache..."
    rm -rf "$HOME/Library/Arduino15/packages/esp32/hardware/esp32/cache"
fi

# Clear any temporary build files
echo "🗑️ Clearing temporary build files..."
rm -rf /tmp/arduino_* 2>/dev/null || true

# Update version reminder
echo "⚠️ Remember to update FIRMWARE_VERSION in your code to \"$VERSION\""
echo "   Press Enter to continue, or Ctrl+C to abort..."
read -r

# Compile firmware
echo "🔨 Compiling firmware with 16MB flash, custom partitions, and OPI PSRAM..."
arduino-cli compile --fqbn "$FQBN" --output-dir ./build .

# Check if compilation succeeded
if [ ! -f "build/Xregulator.ino.bin" ]; then
    echo "❌ Compilation failed - .bin file not found"
    exit 1
fi

FIRMWARE_SIZE=$(wc -c < build/Xregulator.ino.bin)
echo "✅ Firmware compiled: $FIRMWARE_SIZE bytes"

# Create package directory
echo "📦 Creating combined package..."
mkdir -p temp_package
cp build/Xregulator.ino.bin temp_package/firmware.bin

# Copy web files
if [ -d "data" ]; then
    cp -r data/* temp_package/
    echo "✅ Web files added to package"
else
    echo "⚠️ No data folder found - firmware only package"
fi

# Create tar package
cd temp_package
tar --format=ustar -cf "../firmware_$VERSION.tar" .
cd ..
rm -rf temp_package

PACKAGE_SIZE=$(wc -c < "firmware_$VERSION.tar")
echo "✅ Package created: firmware_$VERSION.tar ($PACKAGE_SIZE bytes)"

# Sign package
echo "✏️ Signing package with private key..."
openssl dgst -sha256 -sign "$PRIVATE_KEY" -out "firmware_$VERSION.sig.binary" "firmware_$VERSION.tar"

# Convert signature to base64
base64 -i "firmware_$VERSION.sig.binary" | tr -d '\n' > "firmware_$VERSION.sig.base64"

SIGNATURE_LENGTH=$(wc -c < "firmware_$VERSION.sig.base64")
echo "✅ Package signed: $SIGNATURE_LENGTH character signature"

# Upload files to server
echo "🌐 Uploading to OTA server..."

# Upload firmware package
echo "   📤 Uploading firmware package..."
sshpass -p "$SERVER_PASS" scp "firmware_$VERSION.tar" "$SERVER_USER@$SERVER_HOST:$SERVER_PATH/firmware/"

# Upload signature
echo "   📤 Uploading signature..."
sshpass -p "$SERVER_PASS" ssh "$SERVER_USER@$SERVER_HOST" "echo '$(cat firmware_$VERSION.sig.base64)' > $SERVER_PATH/signatures/firmware_$VERSION.sig"

# Update versions.json with new version info
echo "   📝 Updating versions.json..."
CURRENT_DATE=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
sshpass -p "$SERVER_PASS" ssh "$SERVER_USER@$SERVER_HOST" "
    cd $SERVER_PATH/config
    python3 -c \"
import json
import sys

# Read existing versions
try:
    with open('versions.json', 'r') as f:
        data = json.load(f)
except:
    data = {'versions': {}}

# Add new version
data['versions']['$VERSION'] = {
    'notes': '''$NOTES''',
    'release_date': '$CURRENT_DATE',
    'file_size': $PACKAGE_SIZE
}

# Write back
with open('versions.json', 'w') as f:
    json.dump(data, f, indent=2)
\"
"

# Update server configuration for current version
echo "   ⚙️ Updating server version..."
sshpass -p "$SERVER_PASS" ssh "$SERVER_USER@$SERVER_HOST" "
    cd $SERVER_PATH
    sed -i \"s/CURRENT_FIRMWARE_VERSION', '[^']*'/CURRENT_FIRMWARE_VERSION', '$VERSION'/\" config/ota_config.php
    echo 'Server version updated to $VERSION'
    grep CURRENT_FIRMWARE_VERSION config/ota_config.php
"

# Clear rate limits for testing
echo "   🔄 Clearing rate limits..."
sshpass -p "$SERVER_PASS" ssh "$SERVER_USER@$SERVER_HOST" "rm -f $SERVER_PATH/tmp/rate_limits.json"

# Test new versions API
echo "🧪 Testing versions API..."
curl -s https://ota.xengineering.net/api/firmware/versions.php | python3 -m json.tool

# Cleanup local files
echo "🧹 Cleaning up local files..."
rm -f firmware_$VERSION.sig.binary
rm -f firmware_$VERSION.sig.base64
echo "   (Keeping firmware_$VERSION.tar for reference)"

echo ""
echo "🎉 === DEPLOYMENT COMPLETE ==="
echo "✅ Version $VERSION is now live on the OTA server"
echo "✅ Package size: $PACKAGE_SIZE bytes"
echo "✅ Notes: $NOTES"
echo ""
echo "📱 ESP32 devices can now select this version for manual updates"
