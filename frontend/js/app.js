/**
 * Bank Management System - Main Application Logic
 * Integrates Auth, Staff Management, Lockout handling, and Role Dashboards.
 */

document.addEventListener('DOMContentLoaded', () => {
    initApp();
});

let currentUser = null;

function initApp() {
    setupAuthListeners();
    setupNavigation();
    setupStaffManagement();
    setupModals();

    // Listen for unauthorized events from api.js
    window.addEventListener('bms:unauthorized', () => {
        showLoginPage();
        components.showToast('Your session has expired. Please sign in again.', 'warning');
    });

    // Check existing session
    if (api.isAuthenticated()) {
        currentUser = api.getUser();
        if (currentUser) {
            showAppShell();
        } else {
            showLoginPage();
        }
    } else {
        showLoginPage();
    }
}

/**
 * Switch view to Login Page
 */
function showLoginPage() {
    currentUser = null;
    document.getElementById('page-login').classList.add('active');
    document.getElementById('page-login').classList.remove('hidden');
    document.getElementById('app-shell').classList.add('hidden');
    document.getElementById('login-form').reset();
    components.clearAllErrors('login-form');
    document.getElementById('login-user-id').focus();
}

/**
 * Switch view to App Shell
 */
function showAppShell() {
    document.getElementById('page-login').classList.remove('active');
    document.getElementById('page-login').classList.add('hidden');
    document.getElementById('app-shell').classList.remove('hidden');

    // Update user profile in sidebar
    const nameEl = document.getElementById('user-name');
    const roleEl = document.getElementById('user-role');
    const avatarEl = document.getElementById('user-avatar');
    if (nameEl) nameEl.textContent = currentUser.name || currentUser.id;
    if (roleEl) roleEl.textContent = formatRole(currentUser.role);
    if (avatarEl) avatarEl.textContent = (currentUser.name || currentUser.id).charAt(0).toUpperCase();

    buildNavigation(currentUser.role);
    navigateTo('dashboard');
}

function formatRole(role) {
    if (!role) return '';
    return role.replace(/_/g, ' ').replace(/\b\w/g, l => l.toUpperCase());
}

/**
 * Setup Authentication (Login/Logout)
 */
function setupAuthListeners() {
    const loginForm = document.getElementById('login-form');
    const submitBtn = document.getElementById('login-submit');
    const togglePasswordBtn = document.getElementById('toggle-password');
    const passwordInput = document.getElementById('login-password');
    const logoutBtn = document.getElementById('logout-btn');

    // Toggle password visibility
    if (togglePasswordBtn && passwordInput) {
        togglePasswordBtn.addEventListener('click', () => {
            const isPassword = passwordInput.getAttribute('type') === 'password';
            passwordInput.setAttribute('type', isPassword ? 'text' : 'password');
        });
    }

    // Login submission
    if (loginForm) {
        loginForm.addEventListener('submit', async (e) => {
            e.preventDefault();
            components.clearAllErrors(loginForm);

            const userId = document.getElementById('login-user-id').value.trim();
            const password = passwordInput.value;

            if (!userId) {
                components.setFieldError('login-user-id', 'User ID is required');
                return;
            }
            if (!password) {
                components.setFieldError('login-password', 'Password is required');
                return;
            }

            // Set loading state
            submitBtn.disabled = true;
            submitBtn.querySelector('.btn-text').textContent = 'Signing in...';
            submitBtn.querySelector('.btn-spinner').classList.remove('hidden');

            try {
                const res = await api.login(userId, password, true);
                currentUser = res.user;
                components.showToast(`Welcome back, ${currentUser.name || currentUser.id}!`, 'success');
                showAppShell();
            } catch (err) {
                const generalError = document.getElementById('error-login-general');
                if (err.code === 'ACCOUNT_LOCKED') {
                    // S-09: Display locked account message directing to administrator
                    if (generalError) {
                        generalError.textContent = err.message || 'Account locked after 3 failed login attempts. Please contact an Administrator.';
                        generalError.style.display = 'block';
                    }
                    components.showToast(err.message, 'error', 6000);
                } else if (err.code === 'INVALID_CREDENTIALS') {
                    // S-07: Generic error message, never revealing whether user exists
                    if (generalError) {
                        generalError.textContent = 'Invalid user ID or password.';
                        generalError.style.display = 'block';
                    }
                    components.setFieldError('login-password', 'Invalid credentials');
                } else if (err.field) {
                    components.setFieldError(`login-${err.field}`, err.message);
                } else {
                    if (generalError) {
                        generalError.textContent = err.message || 'Login failed. Please try again.';
                        generalError.style.display = 'block';
                    }
                }
            } finally {
                submitBtn.disabled = false;
                submitBtn.querySelector('.btn-text').textContent = 'Sign In';
                submitBtn.querySelector('.btn-spinner').classList.add('hidden');
            }
        });
    }

    // Logout button (S-08)
    if (logoutBtn) {
        logoutBtn.addEventListener('click', async () => {
            const confirmed = await components.showConfirmDialog({
                title: 'Sign Out',
                message: 'Are you sure you want to sign out?',
                confirmText: 'Sign Out',
                confirmBtnClass: 'btn-danger'
            });

            if (confirmed) {
                await api.logout();
                components.showToast('You have been signed out successfully.', 'info');
                showLoginPage();
            }
        });
    }
}

/**
 * Build dynamic navigation according to user role (S-05, S-25)
 */
function buildNavigation(role) {
    const navLinksContainer = document.getElementById('nav-links');
    if (!navLinksContainer) return;
    navLinksContainer.innerHTML = '';

    const navItems = [
        {
            id: 'dashboard',
            label: 'Dashboard',
            roles: ['administrator', 'bank_officer', 'customer'],
            icon: '<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="3" width="7" height="7"/><rect x="14" y="3" width="7" height="7"/><rect x="14" y="14" width="7" height="7"/><rect x="3" y="14" width="7" height="7"/></svg>'
        },
        {
            id: 'staff',
            label: 'Staff Management',
            roles: ['administrator'],
            icon: '<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M23 21v-2a4 4 0 0 0-3-3.87"/><path d="M16 3.13a4 4 0 0 1 0 7.75"/></svg>'
        },
        {
            id: 'users',
            label: 'User Accounts',
            roles: ['administrator'],
            icon: '<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"/><circle cx="12" cy="7" r="4"/></svg>'
        },
        {
            id: 'transactions',
            label: 'Transactions',
            roles: ['bank_officer'],
            icon: '<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="1" x2="12" y2="23"/><path d="M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6"/></svg>'
        },
        {
            id: 'history',
            label: 'Transaction History',
            roles: ['customer'],
            icon: '<svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/></svg>'
        }
    ];

    navItems.forEach(item => {
        if (item.roles.includes(role)) {
            const li = document.createElement('li');
            li.className = 'nav-item';
            li.innerHTML = `
                <a href="#${item.id}" class="nav-link" data-page="${item.id}">
                    <span class="nav-icon">${item.icon}</span>
                    <span class="nav-label">${item.label}</span>
                </a>
            `;
            li.querySelector('a').addEventListener('click', (e) => {
                e.preventDefault();
                navigateTo(item.id);
            });
            navLinksContainer.appendChild(li);
        }
    });
}

function setupNavigation() {
    window.addEventListener('hashchange', () => {
        const hash = window.location.hash.replace('#', '') || 'dashboard';
        navigateTo(hash);
    });
}

function navigateTo(pageId) {
    // Update active nav link
    document.querySelectorAll('.nav-link').forEach(link => {
        if (link.getAttribute('data-page') === pageId) {
            link.classList.add('active');
        } else {
            link.classList.remove('active');
        }
    });

    // Hide all pages, show target page
    document.querySelectorAll('.content-page').forEach(page => page.classList.add('hidden'));

    const targetPage = document.getElementById(`page-${pageId}`);
    if (targetPage) {
        targetPage.classList.remove('hidden');
    }

    // Load data based on target page
    if (pageId === 'dashboard') {
        loadDashboard();
    } else if (pageId === 'staff') {
        loadStaffList();
    } else if (pageId === 'users') {
        loadUsersList();
    } else if (pageId === 'history') {
        loadCustomerHistory();
    } else if (pageId === 'transactions') {
        loadOfficerTransactions();
    }
}

/**
 * Role-Specific Dashboard (S-25, NFR-2)
 */
async function loadDashboard() {
    const greetingEl = document.getElementById('dashboard-greeting');
    const contentEl = document.getElementById('dashboard-content');
    if (!contentEl) return;

    if (greetingEl && currentUser) {
        greetingEl.textContent = `Welcome back, ${currentUser.name || currentUser.id} • ${formatRole(currentUser.role)}`;
    }

    contentEl.innerHTML = '<div class="loading-spinner">Loading dashboard data...</div>';

    try {
        const data = await api.getDashboard();
        contentEl.innerHTML = '';

        if (currentUser.role === 'administrator') {
            renderAdminDashboard(data, contentEl);
        } else if (currentUser.role === 'bank_officer') {
            renderOfficerDashboard(data, contentEl);
        } else if (currentUser.role === 'customer') {
            renderCustomerDashboard(data, contentEl);
        }
    } catch (err) {
        contentEl.innerHTML = `<div class="error-banner">Failed to load dashboard: ${escapeHtml(err.message)}</div>`;
    }
}

function renderAdminDashboard(data, container) {
    const counts = data.counts || {};
    const today = data.today || {};

    container.innerHTML = `
        <div class="stat-card card-indigo">
            <div class="stat-icon">
                <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M23 21v-2a4 4 0 0 0-3-3.87"/><path d="M16 3.13a4 4 0 0 1 0 7.75"/></svg>
            </div>
            <div class="stat-value">${counts.customers || 0}</div>
            <div class="stat-label">Total Customers</div>
        </div>
        <div class="stat-card card-green">
            <div class="stat-icon">
                <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="2" y="5" width="20" height="14" rx="2"/><line x1="2" y1="10" x2="22" y2="10"/></svg>
            </div>
            <div class="stat-value">${counts.accounts || 0}</div>
            <div class="stat-label">Active Accounts</div>
        </div>
        <div class="stat-card card-amber">
            <div class="stat-icon">
                <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><line x1="19" y1="8" x2="19" y2="14"/><line x1="22" y1="11" x2="16" y2="11"/></svg>
            </div>
            <div class="stat-value">${counts.staff || 0}</div>
            <div class="stat-label">Bank Officers</div>
        </div>
        <div class="stat-card card-cyan">
            <div class="stat-icon">
                <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="22 12 18 12 15 21 9 3 6 12 2 12"/></svg>
            </div>
            <div class="stat-value">₹${formatCurrency(today.total_amount || 0)}</div>
            <div class="stat-label">Today's Volume (${today.transaction_count || 0} txns)</div>
        </div>
    `;
}

function renderOfficerDashboard(data, container) {
    const today = data.today || {};
    const txns = data.recent_processed_transactions || [];

    let txnsHtml = '';
    if (txns.length === 0) {
        txnsHtml = '<div class="table-empty">No transactions processed today.</div>';
    } else {
        txnsHtml = `
            <table class="data-table">
                <thead>
                    <tr><th>Txn ID</th><th>Account</th><th>Type</th><th>Amount</th><th>Time</th></tr>
                </thead>
                <tbody>
                    ${txns.map(t => `
                        <tr>
                            <td class="font-mono">${escapeHtml(t.id)}</td>
                            <td class="font-mono">${escapeHtml(t.account_number)}</td>
                            <td><span class="badge badge-info">${escapeHtml(t.type)}</span></td>
                            <td class="font-semibold">₹${formatCurrency(t.amount)}</td>
                            <td>${formatDate(t.created_at)}</td>
                        </tr>
                    `).join('')}
                </tbody>
            </table>
        `;
    }

    container.innerHTML = `
        <div class="stat-card card-indigo">
            <div class="stat-icon">
                <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><line x1="12" y1="1" x2="12" y2="23"/><path d="M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6"/></svg>
            </div>
            <div class="stat-value">${today.count || 0}</div>
            <div class="stat-label">Transactions Processed Today</div>
        </div>
        <div class="stat-card card-green">
            <div class="stat-icon">
                <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 14 14"/></svg>
            </div>
            <div class="stat-value">₹${formatCurrency(today.total_amount || 0)}</div>
            <div class="stat-label">Total Amount Processed</div>
        </div>
        <div class="dashboard-full-width">
            <div class="card-header">
                <h3>Today's Processed Transactions</h3>
                <button class="btn btn-sm btn-primary" id="btn-demo-deposit">Simulate Deposit</button>
            </div>
            ${txnsHtml}
        </div>
    `;

    const demoDepositBtn = document.getElementById('btn-demo-deposit');
    if (demoDepositBtn) {
        demoDepositBtn.addEventListener('click', async () => {
            // Demonstrate the SRS 3.1 Confirmation Dialog
            const ok = await components.showConfirmDialog({
                title: 'Confirm Cash Deposit',
                message: 'Please review the cash deposit details before posting to the ledger.',
                account: 'ACC-100234',
                amount: 500000,
                resultingBalance: 1250000,
                confirmText: 'Post Deposit'
            });
            if (ok) {
                components.showToast('Cash deposit confirmed and posted successfully.', 'success');
            }
        });
    }
}

function renderCustomerDashboard(data, container) {
    const accounts = data.accounts || [];

    if (accounts.length === 0) {
        container.innerHTML = '<div class="table-empty">No active bank accounts found.</div>';
        return;
    }

    container.innerHTML = `
        <div class="dashboard-full-width">
            <h3 class="mb-3">Your Accounts & Balances</h3>
            <div class="accounts-grid">
                ${accounts.map(acc => `
                    <div class="stat-card card-blue">
                        <div class="stat-icon">
                            <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="2" y="5" width="20" height="14" rx="2"/><line x1="2" y1="10" x2="22" y2="10"/></svg>
                        </div>
                        <div class="account-card-header">
                            <span class="account-type-badge">${escapeHtml(acc.type || 'Savings')}</span>
                            <span class="account-num font-mono">${escapeHtml(acc.account_number)}</span>
                        </div>
                        <div class="stat-value">₹${formatCurrency(acc.balance || 0)}</div>
                        <div class="stat-label">Available Balance</div>
                    </div>
                `).join('')}
            </div>
        </div>
    `;
}

/**
 * Staff Management (S-24)
 */
function setupStaffManagement() {
    const addOfficerBtn = document.getElementById('btn-add-officer');
    const modal = document.getElementById('modal-create-officer');
    const cancelBtn = document.getElementById('cancel-create-officer');
    const form = document.getElementById('create-officer-form');

    if (addOfficerBtn && modal) {
        addOfficerBtn.addEventListener('click', () => {
            components.clearAllErrors(form);
            form.reset();
            modal.classList.remove('hidden');
            document.getElementById('officer-id').focus();
        });
    }

    if (cancelBtn && modal) {
        cancelBtn.addEventListener('click', () => {
            modal.classList.add('hidden');
        });
    }

    if (form) {
        form.addEventListener('submit', async (e) => {
            e.preventDefault();
            components.clearAllErrors(form);

            const id = document.getElementById('officer-id').value.trim();
            const name = document.getElementById('officer-name').value.trim();
            const password = document.getElementById('officer-password').value;

            if (!id) {
                components.setFieldError('officer-id', 'Officer ID is required');
                return;
            }
            if (!name) {
                components.setFieldError('officer-name', 'Full Name is required');
                return;
            }
            if (!password || password.length < 6) {
                components.setFieldError('officer-password', 'Password must be at least 6 characters');
                return;
            }

            try {
                await api.createOfficer({ id, name, password });
                components.showToast(`Bank Officer '${name}' created successfully.`, 'success');
                modal.classList.add('hidden');
                loadStaffList();
            } catch (err) {
                if (err.field) {
                    components.setFieldError(`officer-${err.field}`, err.message);
                } else {
                    components.showToast(err.message || 'Failed to create officer.', 'error');
                }
            }
        });
    }
}

async function loadStaffList() {
    const tbody = document.getElementById('staff-table-body');
    const emptyEl = document.getElementById('staff-table-empty');
    if (!tbody) return;

    tbody.innerHTML = '<tr><td colspan="6" class="text-center">Loading officers...</td></tr>';

    try {
        const res = await api.getStaff();
        const officers = res.officers || [];

        components.renderTable('staff-table-body', officers, (o) => {
            const isActive = o.active !== false && o.active !== 0;
            const isLocked = o.locked === true || o.locked === 1;

            return `
                <tr>
                    <td class="font-mono font-medium">${escapeHtml(o.id)}</td>
                    <td>${escapeHtml(o.name)}</td>
                    <td>
                        <span class="badge ${isActive ? 'badge-success' : 'badge-danger'}">
                            ${isActive ? 'Active' : 'Deactivated'}
                        </span>
                    </td>
                    <td>
                        <span class="badge ${isLocked ? 'badge-warning' : 'badge-neutral'}">
                            ${isLocked ? 'Locked' : 'Normal'}
                        </span>
                    </td>
                    <td>${formatDate(o.created_at)}</td>
                    <td>
                        <div class="row-actions">
                            <button class="btn btn-sm ${isActive ? 'btn-danger' : 'btn-secondary'} btn-toggle-staff"
                                data-id="${escapeHtml(o.id)}"
                                data-name="${escapeHtml(o.name)}"
                                data-active="${isActive}">
                                ${isActive ? 'Deactivate' : 'Activate'}
                            </button>
                        </div>
                    </td>
                </tr>
            `;
        }, 'staff-table-empty');

        // Attach action handlers for deactivating / activating officers
        tbody.querySelectorAll('.btn-toggle-staff').forEach(btn => {
            btn.addEventListener('click', async () => {
                const id = btn.getAttribute('data-id');
                const name = btn.getAttribute('data-name');
                const isCurrentlyActive = btn.getAttribute('data-active') === 'true';
                const action = isCurrentlyActive ? 'deactivate' : 'activate';

                const confirmed = await components.showConfirmDialog({
                    title: `${isCurrentlyActive ? 'Deactivate' : 'Activate'} Bank Officer`,
                    message: `Are you sure you want to ${action} ${name} (${id})? ${isCurrentlyActive ? 'Deactivated officers are logged out immediately and cannot sign in.' : ''}`,
                    confirmText: isCurrentlyActive ? 'Deactivate Officer' : 'Activate Officer',
                    confirmBtnClass: isCurrentlyActive ? 'btn-danger' : 'btn-primary'
                });

                if (confirmed) {
                    try {
                        await api.updateOfficer(id, { active: !isCurrentlyActive });
                        components.showToast(`Officer ${name} ${isCurrentlyActive ? 'deactivated' : 'activated'} successfully.`, 'success');
                        loadStaffList();
                    } catch (err) {
                        components.showToast(err.message || `Failed to ${action} officer.`, 'error');
                    }
                }
            });
        });

    } catch (err) {
        tbody.innerHTML = `<tr><td colspan="6" class="text-danger text-center">Failed to load officers: ${escapeHtml(err.message)}</td></tr>`;
    }
}

/**
 * User Accounts & Admin Unlock (S-09)
 */
async function loadUsersList() {
    const tbody = document.getElementById('users-table-body');
    if (!tbody) return;

    tbody.innerHTML = '<tr><td colspan="6" class="text-center">Loading accounts...</td></tr>';

    try {
        const res = await api.getAllUsers();
        const users = res.users || [];

        components.renderTable('users-table-body', users, (u) => {
            const isLocked = u.locked === true || u.locked === 1;
            const isActive = u.active !== false && u.active !== 0;

            return `
                <tr>
                    <td class="font-mono font-medium">${escapeHtml(u.id)}</td>
                    <td>${escapeHtml(u.name)}</td>
                    <td><span class="badge badge-info">${escapeHtml(formatRole(u.role))}</span></td>
                    <td>
                        <span class="badge ${isActive ? 'badge-success' : 'badge-danger'}">
                            ${isActive ? 'Active' : 'Inactive'}
                        </span>
                    </td>
                    <td>
                        <span class="badge ${isLocked ? 'badge-danger' : 'badge-neutral'}">
                            ${isLocked ? 'Locked (' + (u.failed_attempts || 3) + ' fails)' : 'Normal'}
                        </span>
                    </td>
                    <td>
                        ${isLocked ? `
                            <button class="btn btn-sm btn-primary btn-unlock"
                                data-id="${escapeHtml(u.id)}"
                                data-name="${escapeHtml(u.name)}">
                                Unlock Account
                            </button>
                        ` : '<span class="text-muted text-sm">—</span>'}
                    </td>
                </tr>
            `;
        });

        // Attach unlock handlers
        tbody.querySelectorAll('.btn-unlock').forEach(btn => {
            btn.addEventListener('click', async () => {
                const id = btn.getAttribute('data-id');
                const name = btn.getAttribute('data-name');

                const confirmed = await components.showConfirmDialog({
                    title: 'Unlock User Account',
                    message: `Unlock account for ${name} (${id})? This will reset failed login attempts to 0.`,
                    confirmText: 'Unlock Account',
                    confirmBtnClass: 'btn-primary'
                });

                if (confirmed) {
                    try {
                        await api.unlockUser(id);
                        components.showToast(`Account ${id} unlocked successfully!`, 'success');
                        loadUsersList();
                    } catch (err) {
                        components.showToast(err.message || 'Failed to unlock account.', 'error');
                    }
                }
            });
        });

    } catch (err) {
        tbody.innerHTML = `<tr><td colspan="6" class="text-danger text-center">Failed to load users: ${escapeHtml(err.message)}</td></tr>`;
    }
}

/**
 * Customer Transaction History
 */
async function loadCustomerHistory() {
    const tbody = document.getElementById('history-table-body');
    const emptyEl = document.getElementById('history-table-empty');
    if (!tbody) return;

    try {
        const dashboard = await api.getDashboard();
        const txns = dashboard.recent_transactions || [];

        components.renderTable('history-table-body', txns, (t) => {
            return `
                <tr>
                    <td class="font-mono">${escapeHtml(t.id)}</td>
                    <td class="font-mono">${escapeHtml(t.account_number)}</td>
                    <td><span class="badge badge-info">${escapeHtml(t.type)}</span></td>
                    <td class="font-semibold">₹${formatCurrency(t.amount)}</td>
                    <td class="text-success font-semibold">₹${formatCurrency(t.balance_after)}</td>
                    <td>${formatDate(t.created_at)}</td>
                </tr>
            `;
        }, 'history-table-empty');
    } catch (err) {
        tbody.innerHTML = `<tr><td colspan="6" class="text-danger text-center">Failed to load history: ${escapeHtml(err.message)}</td></tr>`;
    }
}

/**
 * Officer Transactions
 */
function loadOfficerTransactions() {
    const content = document.getElementById('transactions-content');
    if (!content) return;
    content.innerHTML = `
        <div class="card p-4">
            <h3 class="mb-2">Quick Cash Deposit</h3>
            <p class="text-muted mb-4">Process in-branch deposits with instant verification.</p>
            <button class="btn btn-primary" id="btn-officer-deposit-demo">Launch Deposit Flow</button>
        </div>
    `;
    const btn = document.getElementById('btn-officer-deposit-demo');
    if (btn) {
        btn.addEventListener('click', async () => {
            const ok = await components.showConfirmDialog({
                title: 'Confirm Deposit',
                message: 'Verify cash received from customer.',
                account: 'ACC-100234',
                amount: 1000000,
                resultingBalance: 1750000,
                confirmText: 'Process Deposit'
            });
            if (ok) {
                components.showToast('Deposit of ₹10,000.00 processed successfully!', 'success');
            }
        });
    }
}

function setupModals() {
    // Close modal on escape
    document.addEventListener('keydown', (e) => {
        if (e.key === 'Escape') {
            document.querySelectorAll('.dialog-overlay').forEach(el => el.classList.add('hidden'));
        }
    });
}
