/**
 * Bank Management System - Shared UI Components
 * Implements SRS 3.1 requirements:
 * - Reusable form fields with inline errors
 * - Reusable data tables
 * - Confirmation dialog displaying account, amount, and resulting balance
 * - Toast notification system
 */

const components = {
    /**
     * Show a toast message notification
     * @param {string} message
     * @param {'info' | 'success' | 'warning' | 'error'} type
     * @param {number} duration
     */
    showToast(message, type = 'info', duration = 4000) {
        const container = document.getElementById('toast-container');
        if (!container) return;

        const toast = document.createElement('div');
        toast.className = `toast toast-${type}`;
        
        const icons = {
            info: '<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/></svg>',
            success: '<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"/><polyline points="22 4 12 14.01 9 11.01"/></svg>',
            warning: '<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/></svg>',
            error: '<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/></svg>'
        };

        toast.innerHTML = `
            <div class="toast-icon">${icons[type] || icons.info}</div>
            <div class="toast-message">${escapeHtml(message)}</div>
            <button class="toast-close" aria-label="Close">&times;</button>
        `;

        const closeBtn = toast.querySelector('.toast-close');
        closeBtn.addEventListener('click', () => {
            toast.classList.add('fade-out');
            setTimeout(() => toast.remove(), 250);
        });

        container.appendChild(toast);

        if (duration > 0) {
            setTimeout(() => {
                if (toast.parentElement) {
                    toast.classList.add('fade-out');
                    setTimeout(() => toast.remove(), 250);
                }
            }, duration);
        }
    },

    /**
     * Confirmation dialog showing account, amount, and resulting balance (SRS 3.1)
     * @param {Object} options
     * @param {string} options.title - Dialog title
     * @param {string} [options.message] - Optional descriptive message
     * @param {string} [options.account] - Account number or ID
     * @param {number|string} [options.amount] - Transaction amount
     * @param {number|string} [options.resultingBalance] - Resulting balance after transaction
     * @param {string} [options.confirmText] - Label for confirm button
     * @param {string} [options.cancelText] - Label for cancel button
     * @param {string} [options.confirmBtnClass] - CSS class for confirm button
     * @returns {Promise<boolean>}
     */
    showConfirmDialog(options) {
        return new Promise((resolve) => {
            const overlay = document.getElementById('confirm-dialog-overlay');
            const titleEl = document.getElementById('confirm-dialog-title');
            const bodyEl = document.getElementById('confirm-dialog-body');
            const cancelBtn = document.getElementById('confirm-dialog-cancel');
            const okBtn = document.getElementById('confirm-dialog-ok');

            if (!overlay || !titleEl || !bodyEl || !cancelBtn || !okBtn) {
                resolve(confirm(options.title + (options.message ? '\n' + options.message : '')));
                return;
            }

            titleEl.textContent = options.title || 'Confirm Action';
            okBtn.textContent = options.confirmText || 'Confirm';
            cancelBtn.textContent = options.cancelText || 'Cancel';

            okBtn.className = options.confirmBtnClass ? `btn ${options.confirmBtnClass}` : 'btn btn-primary';

            let bodyHtml = '';
            if (options.message) {
                bodyHtml += `<p class="dialog-description">${escapeHtml(options.message)}</p>`;
            }

            // SRS 3.1: Confirmation dialog showing account, amount and resulting balance
            if (options.account || options.amount !== undefined || options.resultingBalance !== undefined) {
                bodyHtml += `
                    <div class="transaction-summary-card">
                        ${options.account ? `
                            <div class="summary-row">
                                <span class="summary-label">Account</span>
                                <span class="summary-val font-mono">${escapeHtml(String(options.account))}</span>
                            </div>` : ''}
                        ${options.amount !== undefined ? `
                            <div class="summary-row">
                                <span class="summary-label">Amount</span>
                                <span class="summary-val font-semibold text-highlight">₹${formatCurrency(options.amount)}</span>
                            </div>` : ''}
                        ${options.resultingBalance !== undefined ? `
                            <div class="summary-row resulting-balance">
                                <span class="summary-label">Resulting Balance</span>
                                <span class="summary-val font-bold text-success">₹${formatCurrency(options.resultingBalance)}</span>
                            </div>` : ''}
                    </div>
                `;
            }

            bodyEl.innerHTML = bodyHtml;
            overlay.classList.remove('hidden');

            const cleanup = () => {
                overlay.classList.add('hidden');
                cancelBtn.removeEventListener('click', onCancel);
                okBtn.removeEventListener('click', onConfirm);
                document.removeEventListener('keydown', onKeyDown);
            };

            const onConfirm = () => {
                cleanup();
                resolve(true);
            };

            const onCancel = () => {
                cleanup();
                resolve(false);
            };

            const onKeyDown = (e) => {
                if (e.key === 'Escape') onCancel();
                if (e.key === 'Enter') onConfirm();
            };

            cancelBtn.addEventListener('click', onCancel);
            okBtn.addEventListener('click', onConfirm);
            document.addEventListener('keydown', onKeyDown);
            okBtn.focus();
        });
    },

    /**
     * Form field error handling
     */
    setFieldError(inputElOrId, errorMessage) {
        const input = typeof inputElOrId === 'string' ? document.getElementById(inputElOrId) : inputElOrId;
        if (!input) return;

        input.classList.add('input-error');
        const formField = input.closest('.form-field') || input.parentElement;
        if (formField) {
            let errorEl = formField.querySelector('.field-error');
            if (!errorEl) {
                errorEl = document.createElement('span');
                errorEl.className = 'field-error';
                formField.appendChild(errorEl);
            }
            errorEl.textContent = errorMessage || '';
            errorEl.style.display = errorMessage ? 'block' : 'none';
        }
    },

    clearFieldError(inputElOrId) {
        const input = typeof inputElOrId === 'string' ? document.getElementById(inputElOrId) : inputElOrId;
        if (!input) return;

        input.classList.remove('input-error');
        const formField = input.closest('.form-field') || input.parentElement;
        if (formField) {
            const errorEl = formField.querySelector('.field-error');
            if (errorEl) {
                errorEl.textContent = '';
                errorEl.style.display = 'none';
            }
        }
    },

    clearAllErrors(containerElOrId) {
        const container = typeof containerElOrId === 'string' ? document.getElementById(containerElOrId) : containerElOrId;
        if (!container) return;

        container.querySelectorAll('.input-error').forEach(el => el.classList.remove('input-error'));
        container.querySelectorAll('.field-error').forEach(el => {
            el.textContent = '';
            el.style.display = 'none';
        });
    },

    /**
     * Reusable Data Table Renderer
     * @param {string} tbodyId
     * @param {Array<Object>} data
     * @param {Function} rowRenderer - returns HTML string for <tr>
     * @param {string} [emptyMsgId]
     */
    renderTable(tbodyId, data, rowRenderer, emptyMsgId) {
        const tbody = document.getElementById(tbodyId);
        const emptyEl = emptyMsgId ? document.getElementById(emptyMsgId) : null;
        if (!tbody) return;

        if (!data || data.length === 0) {
            tbody.innerHTML = '';
            if (emptyEl) emptyEl.classList.remove('hidden');
            return;
        }

        if (emptyEl) emptyEl.classList.add('hidden');
        tbody.innerHTML = data.map(rowRenderer).join('');
    }
};

function escapeHtml(str) {
    if (str === null || str === undefined) return '';
    return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
}

function formatCurrency(amount) {
    const num = Number(amount);
    if (isNaN(num)) return '0.00';
    return (num / 100).toLocaleString('en-IN', {
        minimumFractionDigits: 2,
        maximumFractionDigits: 2
    });
}

function formatDate(isoStr) {
    if (!isoStr) return '-';
    try {
        const d = new Date(isoStr);
        return d.toLocaleDateString('en-IN', {
            year: 'numeric',
            month: 'short',
            day: 'numeric',
            hour: '2-digit',
            minute: '2-digit'
        });
    } catch {
        return isoStr;
    }
}

window.components = components;
window.escapeHtml = escapeHtml;
window.formatCurrency = formatCurrency;
window.formatDate = formatDate;
