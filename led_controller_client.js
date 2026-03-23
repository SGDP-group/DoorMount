/**
 * ESP32 LED Controller - JavaScript Client Library
 * 
 * Use this in your web applications or Node.js projects
 * 
 * Example:
 *   const led = new LEDController('192.168.1.100');
 *   await led.setColor(255, 0, 0);  // Turn red
 */

class LEDController {
  constructor(ipAddress, port = 80) {
    this.baseUrl = `http://${ipAddress}:${port}`;
    this.timeout = 5000;
    
    // Color presets
    this.colors = {
      red: { r: 255, g: 0, b: 0 },
      green: { r: 0, g: 255, b: 0 },
      blue: { r: 0, g: 0, b: 255 },
      yellow: { r: 255, g: 255, b: 0 },
      cyan: { r: 0, g: 255, b: 255 },
      magenta: { r: 255, g: 0, b: 255 },
      white: { r: 255, g: 255, b: 255 },
      off: { r: 0, g: 0, b: 0 }
    };
  }

  /**
   * Set LED to specific RGB color
   * @param {number} red - Red value (0-255)
   * @param {number} green - Green value (0-255)
   * @param {number} blue - Blue value (0-255)
   * @returns {Promise<Object>} Response from device
   */
  async setColor(red, green, blue) {
    const payload = {
      red: Math.max(0, Math.min(255, red)),
      green: Math.max(0, Math.min(255, green)),
      blue: Math.max(0, Math.min(255, blue))
    };

    try {
      const response = await this._post('/color', payload);
      console.log(`LED set to RGB(${payload.red}, ${payload.green}, ${payload.blue})`);
      return response;
    } catch (error) {
      console.error('Failed to set color:', error.message);
      throw error;
    }
  }

  /**
   * Set LED to preset color by name
   * @param {string} colorName - Name of preset color
   * @returns {Promise<Object>} Response from device
   */
  async setColorByName(colorName) {
    colorName = colorName.toLowerCase();
    if (!this.colors[colorName]) {
      throw new Error(`Unknown color: ${colorName}`);
    }

    const { r, g, b } = this.colors[colorName];
    return this.setColor(r, g, b);
  }

  /**
   * Flash LED with specified color
   * @param {number} red - Red value (0-255)
   * @param {number} green - Green value (0-255)
   * @param {number} blue - Blue value (0-255)
   * @param {number} flashes - Number of flashes (default: 3)
   * @param {number} duration - Duration per flash in ms (default: 500)
   * @returns {Promise<Object>} Response from device
   */
  async flash(red, green, blue, flashes = 3, duration = 500) {
    const payload = {
      red: Math.max(0, Math.min(255, red)),
      green: Math.max(0, Math.min(255, green)),
      blue: Math.max(0, Math.min(255, blue)),
      flashes: Math.max(1, Math.min(100, flashes)),
      duration: Math.max(50, Math.min(5000, duration))
    };

    try {
      const response = await this._post('/flash', payload);
      console.log(`LED flashed RGB(${payload.red}, ${payload.green}, ${payload.blue}) ${payload.flashes}x`);
      return response;
    } catch (error) {
      console.error('Failed to flash:', error.message);
      throw error;
    }
  }

  /**
   * Flash LED with preset color
   * @param {string} colorName - Name of preset color
   * @param {number} flashes - Number of flashes
   * @param {number} duration - Duration per flash in ms
   * @returns {Promise<Object>} Response from device
   */
  async flashByName(colorName, flashes = 3, duration = 500) {
    colorName = colorName.toLowerCase();
    if (!this.colors[colorName]) {
      throw new Error(`Unknown color: ${colorName}`);
    }

    const { r, g, b } = this.colors[colorName];
    return this.flash(r, g, b, flashes, duration);
  }

  /**
   * Get device status
   * @returns {Promise<Object>} Device status
   */
  async getStatus() {
    try {
      const status = await this._get('/status');
      console.log('Device Status:', status);
      return status;
    } catch (error) {
      console.error('Failed to get status:', error.message);
      throw error;
    }
  }

  /**
   * Test connection to device
   * @returns {Promise<boolean>} True if device is reachable
   */
  async isOnline() {
    try {
      await this.getStatus();
      return true;
    } catch {
      return false;
    }
  }

  /**
   * Test all color presets
   * @returns {Promise<void>}
   */
  async testAllColors() {
    console.log('Testing all colors...');
    for (const [colorName] of Object.entries(this.colors)) {
      await this.setColorByName(colorName);
      await this._delay(500);
    }
    console.log('Color test complete!');
  }

  /**
   * Cycle through rainbow colors
   * @param {number} duration - Duration for each color in ms
   * @returns {Promise<void>}
   */
  async rainbowCycle(duration = 1000) {
    console.log('Starting rainbow cycle...');
    const sequence = ['red', 'yellow', 'green', 'cyan', 'blue', 'magenta', 'red'];

    for (const color of sequence) {
      await this.setColorByName(color);
      await this._delay(duration);
    }
  }

  /**
   * Pulse (fade) effect - requires multiple calls
   * @param {string} colorName - Color to pulse
   * @param {number} cycles - Number of pulse cycles
   * @returns {Promise<void>}
   */
  async pulse(colorName, cycles = 3) {
    colorName = colorName.toLowerCase();
    if (!this.colors[colorName]) {
      throw new Error(`Unknown color: ${colorName}`);
    }

    const { r, g, b } = this.colors[colorName];
    
    for (let i = 0; i < cycles; i++) {
      // Fade in
      for (let brightness = 0; brightness <= 255; brightness += 25) {
        await this.setColor(
          Math.floor(r * brightness / 255),
          Math.floor(g * brightness / 255),
          Math.floor(b * brightness / 255)
        );
        await this._delay(50);
      }
      
      // Fade out
      for (let brightness = 255; brightness >= 0; brightness -= 25) {
        await this.setColor(
          Math.floor(r * brightness / 255),
          Math.floor(g * brightness / 255),
          Math.floor(b * brightness / 255)
        );
        await this._delay(50);
      }
    }
  }

  // Private methods

  async _post(endpoint, payload) {
    const response = await fetch(`${this.baseUrl}${endpoint}`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(payload)
    });

    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`);
    }

    return response.json();
  }

  async _get(endpoint) {
    const response = await fetch(`${this.baseUrl}${endpoint}`);

    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`);
    }

    return response.json();
  }

  _delay(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
  }
}

// Export for Node.js
if (typeof module !== 'undefined' && module.exports) {
  module.exports = LEDController;
}

// Example usage
/*
(async () => {
  const led = new LEDController('192.168.1.100');

  // Check if device is online
  if (await led.isOnline()) {
    console.log('Device is online!');
    
    // Set colors
    await led.setColor(255, 0, 0);    // Red
    await led.setColor(0, 255, 0);    // Green
    
    // Use preset colors
    await led.setColorByName('blue');
    
    // Flash a color
    await led.flash(255, 255, 0, 3, 500);  // Yellow flash
    
    // Test all colors
    await led.testAllColors();
    
    // Rainbow cycle
    await led.rainbowCycle(1000);
  } else {
    console.error('Device is offline');
  }
})();
*/
