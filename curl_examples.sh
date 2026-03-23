#!/bin/bash
# ESP32 LED Controller - cURL Usage Examples
# 
# Usage:
#   bash curl_examples.sh <ESP32_IP_ADDRESS>
#
# Example:
#   bash curl_examples.sh 192.168.1.100

if [ -z "$1" ]; then
    echo "Usage: bash curl_examples.sh <ESP32_IP_ADDRESS>"
    echo "Example: bash curl_examples.sh 192.168.1.100"
    exit 1
fi

ESP32_IP="$1"
BASE_URL="http://$ESP32_IP"

echo "========================================="
echo "ESP32 LED Controller - cURL Examples"
echo "Target: $BASE_URL"
echo "========================================="
echo ""

# Test connection
echo "1. Testing connection..."
curl -s "$BASE_URL/status" | jq . || echo "Failed to connect"
echo ""

# Get documentation
echo "2. Getting API documentation..."
curl -s "$BASE_URL/" | head -20
echo "..."
echo ""

# Set color - Red
echo "3. Setting LED to RED..."
curl -X POST "$BASE_URL/color" \
  -H "Content-Type: application/json" \
  -d '{"red": 255, "green": 0, "blue": 0}' \
  -s | jq .
sleep 1
echo ""

# Set color - Green
echo "4. Setting LED to GREEN..."
curl -X POST "$BASE_URL/color" \
  -H "Content-Type: application/json" \
  -d '{"red": 0, "green": 255, "blue": 0}' \
  -s | jq .
sleep 1
echo ""

# Set color - Blue
echo "5. Setting LED to BLUE..."
curl -X POST "$BASE_URL/color" \
  -H "Content-Type: application/json" \
  -d '{"red": 0, "green": 0, "blue": 255}' \
  -s | jq .
sleep 1
echo ""

# Flash - Yellow
echo "6. Flashing YELLOW (3 times)..."
curl -X POST "$BASE_URL/flash" \
  -H "Content-Type: application/json" \
  -d '{"red": 255, "green": 255, "blue": 0, "flashes": 3, "duration": 500}' \
  -s | jq .
sleep 3
echo ""

# Set color - White
echo "7. Setting LED to WHITE..."
curl -X POST "$BASE_URL/color" \
  -H "Content-Type: application/json" \
  -d '{"red": 255, "green": 255, "blue": 255}' \
  -s | jq .
sleep 1
echo ""

# Flash - Cyan
echo "8. Flashing CYAN (5 times)..."
curl -X POST "$BASE_URL/flash" \
  -H "Content-Type: application/json" \
  -d '{"red": 0, "green": 255, "blue": 255, "flashes": 5, "duration": 300}' \
  -s | jq .
sleep 3
echo ""

# Turn off
echo "9. Turning LED OFF..."
curl -X POST "$BASE_URL/color" \
  -H "Content-Type: application/json" \
  -d '{"red": 0, "green": 0, "blue": 0}' \
  -s | jq .
echo ""

# Get final status
echo "10. Final device status..."
curl -s "$BASE_URL/status" | jq .
echo ""

echo "========================================="
echo "Examples complete!"
echo "========================================="
