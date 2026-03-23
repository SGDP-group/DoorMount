#!/usr/bin/env python3
"""
ESP32 LED Controller API Test Script

Usage:
    python test_api.py <ESP32_IP_ADDRESS>
    
Example:
    python test_api.py 192.168.1.100
"""

import requests
import json
import sys
import time
from typing import Tuple

class LEDController:
    """Test client for ESP32 LED Controller API"""
    
    # Color presets
    COLORS = {
        'red': (255, 0, 0),
        'green': (0, 255, 0),
        'blue': (0, 0, 255),
        'yellow': (255, 255, 0),
        'cyan': (0, 255, 255),
        'magenta': (255, 0, 255),
        'white': (255, 255, 255),
        'off': (0, 0, 0),
    }
    
    def __init__(self, ip_address: str, timeout: int = 5):
        """Initialize LED controller client"""
        self.ip = ip_address
        self.base_url = f"http://{ip_address}"
        self.timeout = timeout
    
    def set_color(self, red: int, green: int, blue: int) -> bool:
        """Set LED to specific RGB color"""
        try:
            payload = {
                'red': max(0, min(255, red)),
                'green': max(0, min(255, green)),
                'blue': max(0, min(255, blue))
            }
            response = requests.post(
                f"{self.base_url}/color",
                json=payload,
                timeout=self.timeout
            )
            
            if response.status_code == 200:
                print(f"✓ LED set to RGB({payload['red']}, {payload['green']}, {payload['blue']})")
                return True
            else:
                print(f"✗ Failed to set color: {response.status_code}")
                return False
                
        except requests.exceptions.RequestException as e:
            print(f"✗ Connection error: {e}")
            return False
    
    def set_color_by_name(self, color_name: str) -> bool:
        """Set LED to preset color by name"""
        color_name = color_name.lower()
        if color_name not in self.COLORS:
            print(f"✗ Unknown color: {color_name}")
            print(f"  Available: {', '.join(self.COLORS.keys())}")
            return False
        
        r, g, b = self.COLORS[color_name]
        return self.set_color(r, g, b)
    
    def flash(self, red: int, green: int, blue: int, 
              flashes: int = 3, duration: int = 500) -> bool:
        """Flash LED with specified color"""
        try:
            payload = {
                'red': max(0, min(255, red)),
                'green': max(0, min(255, green)),
                'blue': max(0, min(255, blue)),
                'flashes': max(1, min(100, flashes)),
                'duration': max(50, min(5000, duration))
            }
            response = requests.post(
                f"{self.base_url}/flash",
                json=payload,
                timeout=self.timeout + (duration * payload['flashes'] / 1000)
            )
            
            if response.status_code == 200:
                print(f"✓ LED flashed RGB({payload['red']}, {payload['green']}, {payload['blue']}) "
                      f"{payload['flashes']}x ({payload['duration']}ms)")
                return True
            else:
                print(f"✗ Failed to flash: {response.status_code}")
                return False
                
        except requests.exceptions.RequestException as e:
            print(f"✗ Connection error: {e}")
            return False
    
    def flash_by_name(self, color_name: str, flashes: int = 3, 
                      duration: int = 500) -> bool:
        """Flash LED with preset color"""
        color_name = color_name.lower()
        if color_name not in self.COLORS:
            print(f"✗ Unknown color: {color_name}")
            return False
        
        r, g, b = self.COLORS[color_name]
        return self.flash(r, g, b, flashes, duration)
    
    def get_status(self) -> dict:
        """Get device status"""
        try:
            response = requests.get(
                f"{self.base_url}/status",
                timeout=self.timeout
            )
            
            if response.status_code == 200:
                status = response.json()
                print(f"✓ Device Status:")
                print(f"  IP: {status.get('ip')}")
                print(f"  SSID: {status.get('ssid')}")
                print(f"  Signal: {status.get('signal_strength')} dBm")
                print(f"  Status: {status.get('status')}")
                return status
            else:
                print(f"✗ Failed to get status: {response.status_code}")
                return {}
                
        except requests.exceptions.RequestException as e:
            print(f"✗ Connection error: {e}")
            return {}
    
    def test_all_colors(self) -> bool:
        """Test all color presets"""
        print("\n=== Testing All Colors ===")
        all_success = True
        
        for color_name in self.COLORS:
            if not self.set_color_by_name(color_name):
                all_success = False
            time.sleep(0.5)
        
        return all_success
    
    def rainbow_cycle(self, duration: int = 1000):
        """Cycle through rainbow colors"""
        print("\n=== Rainbow Cycle ===")
        colors = ['red', 'yellow', 'green', 'cyan', 'blue', 'magenta', 'red']
        
        for color in colors:
            self.set_color_by_name(color)
            time.sleep(duration / 1000)
    
    def interactive_mode(self):
        """Interactive command mode"""
        print("\n=== Interactive Mode ===")
        print("Commands:")
        print("  color <name>              - Set color by name (red, green, blue, etc.)")
        print("  rgb <r> <g> <b>          - Set RGB color (0-255)")
        print("  flash <name> [count] [ms] - Flash color")
        print("  status                    - Get device status")
        print("  colors                    - List all color presets")
        print("  test                      - Test all colors")
        print("  rainbow                   - Cycle rainbow colors")
        print("  exit                      - Exit interactive mode")
        print()
        
        while True:
            try:
                cmd = input("> ").strip().lower().split()
                
                if not cmd:
                    continue
                
                if cmd[0] == 'exit':
                    break
                elif cmd[0] == 'color' and len(cmd) > 1:
                    self.set_color_by_name(cmd[1])
                elif cmd[0] == 'rgb' and len(cmd) >= 4:
                    self.set_color(int(cmd[1]), int(cmd[2]), int(cmd[3]))
                elif cmd[0] == 'flash' and len(cmd) > 1:
                    flashes = int(cmd[2]) if len(cmd) > 2 else 3
                    duration = int(cmd[3]) if len(cmd) > 3 else 500
                    self.flash_by_name(cmd[1], flashes, duration)
                elif cmd[0] == 'status':
                    self.get_status()
                elif cmd[0] == 'colors':
                    print(f"Available colors: {', '.join(self.COLORS.keys())}")
                elif cmd[0] == 'test':
                    self.test_all_colors()
                elif cmd[0] == 'rainbow':
                    self.rainbow_cycle()
                else:
                    print("Unknown command")
                    
            except (ValueError, IndexError) as e:
                print(f"Invalid command: {e}")
            except KeyboardInterrupt:
                print("\nExit")
                break


def main():
    """Main function"""
    if len(sys.argv) < 2:
        print("Usage: python test_api.py <ESP32_IP_ADDRESS> [--interactive]")
        print("\nExamples:")
        print("  python test_api.py 192.168.1.100")
        print("  python test_api.py 192.168.1.100 --interactive")
        sys.exit(1)
    
    ip_address = sys.argv[1]
    interactive = '--interactive' in sys.argv
    
    print(f"ESP32 LED Controller API Tester")
    print(f"Target: {ip_address}\n")
    
    controller = LEDController(ip_address)
    
    # Test connection
    print("Testing connection...")
    status = controller.get_status()
    if not status:
        print("\n✗ Cannot connect to device. Check IP address and network connection.")
        sys.exit(1)
    
    if interactive:
        controller.interactive_mode()
    else:
        # Run demo sequence
        print("\n=== Demo Sequence ===\n")
        
        controller.set_color_by_name('red')
        time.sleep(1)
        
        controller.set_color_by_name('green')
        time.sleep(1)
        
        controller.set_color_by_name('blue')
        time.sleep(1)
        
        controller.set_color_by_name('white')
        time.sleep(1)
        
        controller.flash_by_name('yellow', 3, 500)
        time.sleep(1)
        
        controller.set_color_by_name('off')
        print("\n✓ Demo complete!")
        print("\nRun with --interactive flag for interactive testing:")
        print(f"  python test_api.py {ip_address} --interactive")


if __name__ == '__main__':
    main()
