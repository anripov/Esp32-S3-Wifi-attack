class SecurityDashboard {
    constructor() {
        this.selectedNetwork = null;
        this.initializeEventListeners();
        this.initializeFormValidation();
        this.loadClientMacFromURL();
        this.startAutoRefresh();
    }

    initializeEventListeners() {
        // Attack form submission
        document.getElementById('attack-form').addEventListener('submit', (e) => this.handleAttackSubmit(e));

        // Scan form submission
        document.getElementById('scan-form').addEventListener('submit', (e) => this.handleScanSubmit(e));

        // Form input changes
        document.getElementById('duration').addEventListener('input', (e) => this.validateDuration(e));
        document.getElementById('client_mac').addEventListener('input', (e) => this.validateClientMac(e));
    }

    initializeFormValidation() {
        // Add real-time validation feedback
        const inputs = document.querySelectorAll('input[required]');
        inputs.forEach(input => {
            input.addEventListener('blur', () => this.validateInput(input));
            input.addEventListener('input', () => this.clearInputError(input));
        });
    }

    setTarget(ssid, bssid, ch, encryption) {
        this.selectedNetwork = { ssid, bssid, ch, encryption };

        // Update attack form
        document.getElementById('ssid').value = ssid;
        document.getElementById('bssid').value = bssid;
        document.getElementById('ch').value = ch;

        // Update scan form
        document.getElementById('scan_ssid').value = ssid;
        document.getElementById('scan_bssid').value = bssid;
        document.getElementById('scan_ch').value = ch;

        // Enable readonly fields
        this.enableForms();

        // Visual feedback
        this.highlightSelectedNetwork(ssid);
        this.showSelectionFeedback();

        // Scroll to configuration
        document.getElementById('attack-form').scrollIntoView({
            behavior: 'smooth',
            block: 'center'
        });
    }

    enableForms() {
        const readonlyInputs = document.querySelectorAll('input[readonly]');
        readonlyInputs.forEach(input => {
            input.style.backgroundColor = 'var(--surface-primary)';
            input.style.borderColor = 'var(--success-color)';
        });

        // Enable submit buttons
        document.querySelector('#attack-form button').disabled = false;
        document.querySelector('#scan-form button').disabled = false;
    }

    highlightSelectedNetwork(ssid) {
        // Remove previous highlights
        document.querySelectorAll('tr.selected').forEach(row => {
            row.classList.remove('selected');
        });

        // Highlight current selection
        const rows = document.querySelectorAll('tbody tr');
        rows.forEach(row => {
            const firstCell = row.querySelector('td');
            if (firstCell && firstCell.textContent.trim() === ssid) {
                row.classList.add('selected');
                row.style.backgroundColor = 'rgba(37, 99, 235, 0.1)';
                row.style.borderLeft = '4px solid var(--primary-color)';
            }
        });
    }

    showSelectionFeedback() {
        // Create or update selection indicator
        let indicator = document.getElementById('selection-indicator');
        if (!indicator) {
            indicator = document.createElement('div');
            indicator.id = 'selection-indicator';
            indicator.className = 'alert alert-success';
            indicator.style.position = 'fixed';
            indicator.style.top = '20px';
            indicator.style.right = '20px';
            indicator.style.zIndex = '1000';
            indicator.style.minWidth = '300px';
            document.body.appendChild(indicator);
        }

        indicator.innerHTML = `
            <svg class="icon" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
                <path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 15l-5-5 1.41-1.41L10 14.17l7.59-7.59L19 8l-9 9z"/>
            </svg>
            Target selected: <strong>${this.selectedNetwork.ssid}</strong>
        `;

        // Auto-hide after 3 seconds
        setTimeout(() => {
            if (indicator) {
                indicator.style.opacity = '0';
                setTimeout(() => indicator.remove(), 300);
            }
        }, 3000);
    }

    handleAttackSubmit(e) {
        if (!this.selectedNetwork) {
            e.preventDefault();
            this.showError('Please select a target network first');
            return;
        }

        const duration = parseInt(document.getElementById('duration').value);
        if (duration < 5 || duration > 60) {
            e.preventDefault();
            this.showError('Duration must be between 5 and 60 seconds');
            return;
        }

        // Show confirmation dialog
        const confirmMessage = `
            Launch security assessment on:
            Network: ${this.selectedNetwork.ssid}
            Duration: ${duration} seconds

            This action should only be performed on networks you own or have explicit authorization to test.

            Continue?
        `;

        if (!confirm(confirmMessage)) {
            e.preventDefault();
            return;
        }

        // Show loading state
        this.showLoadingState('Launching security assessment...');
    }

    handleScanSubmit(e) {
        if (!this.selectedNetwork) {
            e.preventDefault();
            this.showError('Please select a target network first');
            return;
        }

        this.showLoadingState('Scanning for connected devices...');
    }

    validateDuration(e) {
        const value = parseInt(e.target.value);
        const feedback = e.target.parentNode.querySelector('.feedback') || this.createFeedback(e.target);

        if (value < 5) {
            feedback.textContent = 'Minimum duration is 5 seconds';
            feedback.className = 'feedback error';
        } else if (value > 60) {
            feedback.textContent = 'Maximum duration is 60 seconds';
            feedback.className = 'feedback error';
        } else {
            feedback.textContent = 'Duration is valid';
            feedback.className = 'feedback success';
        }
    }

    validateClientMac(e) {
        const value = e.target.value.trim();
        if (!value) return; // Optional field

        const macPattern = /^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$/;
        const feedback = e.target.parentNode.querySelector('.feedback') || this.createFeedback(e.target);

        if (macPattern.test(value)) {
            feedback.textContent = 'Valid MAC address format';
            feedback.className = 'feedback success';
        } else {
            feedback.textContent = 'Invalid MAC address format (use XX:XX:XX:XX:XX:XX)';
            feedback.className = 'feedback error';
        }
    }

    createFeedback(input) {
        const feedback = document.createElement('small');
        feedback.className = 'feedback';
        feedback.style.display = 'block';
        feedback.style.marginTop = '4px';
        feedback.style.fontSize = '0.75rem';
        input.parentNode.appendChild(feedback);
        return feedback;
    }

    validateInput(input) {
        if (input.hasAttribute('required') && !input.value.trim()) {
            this.showInputError(input, 'This field is required');
            return false;
        }
        return true;
    }

    showInputError(input, message) {
        input.style.borderColor = 'var(--error-color)';
        input.style.boxShadow = '0 0 0 3px rgba(220, 38, 38, 0.1)';

        let errorMsg = input.parentNode.querySelector('.error-message');
        if (!errorMsg) {
            errorMsg = document.createElement('small');
            errorMsg.className = 'error-message';
            errorMsg.style.color = 'var(--error-color)';
            errorMsg.style.fontSize = '0.75rem';
            errorMsg.style.display = 'block';
            errorMsg.style.marginTop = '4px';
            input.parentNode.appendChild(errorMsg);
        }
        errorMsg.textContent = message;
    }

    clearInputError(input) {
        input.style.borderColor = '';
        input.style.boxShadow = '';

        const errorMsg = input.parentNode.querySelector('.error-message');
        if (errorMsg) {
            errorMsg.remove();
        }
    }

    showError(message) {
        const errorDiv = document.createElement('div');
        errorDiv.className = 'alert alert-error';
        errorDiv.style.position = 'fixed';
        errorDiv.style.top = '20px';
        errorDiv.style.left = '50%';
        errorDiv.style.transform = 'translateX(-50%)';
        errorDiv.style.zIndex = '1000';
        errorDiv.style.minWidth = '400px';
        errorDiv.innerHTML = `
            <svg class="icon" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
                <path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-2h2v2zm0-4h-2V7h2v6z"/>
            </svg>
            ${message}
        `;

        document.body.appendChild(errorDiv);

        setTimeout(() => {
            errorDiv.style.opacity = '0';
            setTimeout(() => errorDiv.remove(), 300);
        }, 4000);
    }

    showLoadingState(message) {
        const overlay = document.createElement('div');
        overlay.id = 'loading-overlay';
        overlay.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(15, 23, 42, 0.8);
            backdrop-filter: blur(8px);
            z-index: 9999;
            display: flex;
            align-items: center;
            justify-content: center;
        `;

        overlay.innerHTML = `
            <div style="background: var(--surface-elevated); padding: var(--spacing-xl); border-radius: var(--border-radius-lg); box-shadow: var(--shadow-xl); text-align: center; min-width: 300px;">
                <div class="spinner" style="margin-bottom: var(--spacing-md);"></div>
                <p style="margin: 0; color: var(--text-primary); font-weight: 500;">${message}</p>
            </div>
        `;

        document.body.appendChild(overlay);
    }

    loadClientMacFromURL() {
        const params = new URLSearchParams(window.location.search);
        const clientMac = params.get('client_mac');
        if (clientMac) {
            document.getElementById('client_mac').value = clientMac;
            document.getElementById('client_mac').scrollIntoView({ behavior: 'smooth' });

            // Show notification
            setTimeout(() => {
                this.showError('Client MAC loaded from previous scan: ' + clientMac);
            }, 1000);
        }
    }

    startAutoRefresh() {
        // Auto-refresh network list every 30 seconds if no forms are being filled
        setInterval(() => {
            const activeElement = document.activeElement;
            const isFormActive = activeElement && (activeElement.tagName === 'INPUT' || activeElement.tagName === 'TEXTAREA');

            if (!isFormActive && !this.selectedNetwork) {
                location.reload();
            }
        }, 30000);
    }
}

// Global function for backward compatibility
function setTarget(ssid, bssid, ch, encryption) {
    if (window.dashboard) {
        window.dashboard.setTarget(ssid, bssid, ch, encryption);
    }
}

// Initialize dashboard when DOM is loaded
document.addEventListener('DOMContentLoaded', function() {
    window.dashboard = new SecurityDashboard();

    // Add CSS for feedback and animations
    const style = document.createElement('style');
    style.textContent = `
        .feedback.success { color: var(--success-color); }
        .feedback.error { color: var(--error-color); }
        tr.selected { transition: all 0.3s ease; }
        .alert { transition: opacity 0.3s ease; }
        .spinner {
            border: 3px solid rgba(37, 99, 235, 0.3);
            width: 24px;
            height: 24px;
            border-radius: 50%;
            border-left-color: var(--primary-color);
            animation: spin 1s linear infinite;
            margin: 0 auto;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
    `;
    document.head.appendChild(style);
});
