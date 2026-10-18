/**
 * Bank Management System - API Client
 * Handles authenticated HTTP requests, session storage, and error normalization.
 */

const API_BASE = '';

const api = {
    // Session token storage helpers
    getToken() {
        return localStorage.getItem('bms_token') || sessionStorage.getItem('bms_token');
    },

    setToken(token, remember = false) {
        if (remember) {
            localStorage.setItem('bms_token', token);
        } else {
            sessionStorage.setItem('bms_token', token);
        }
    },

    getUser() {
        const u = localStorage.getItem('bms_user') || sessionStorage.getItem('bms_user');
        return u ? JSON.parse(u) : null;
    },

    setUser(user, remember = false) {
        const s = JSON.stringify(user);
        if (remember) {
            localStorage.setItem('bms_user', s);
        } else {
            sessionStorage.setItem('bms_user', s);
        }
    },

    clearSession() {
        localStorage.removeItem('bms_token');
        localStorage.removeItem('bms_user');
        sessionStorage.removeItem('bms_token');
        sessionStorage.removeItem('bms_user');
    },

    isAuthenticated() {
        return !!this.getToken();
    },

    /**
     * Core request wrapper
     */
    async request(endpoint, options = {}) {
        const url = `${API_BASE}${endpoint}`;
        const headers = {
            'Accept': 'application/json',
            ...(options.headers || {})
        };

        const token = this.getToken();
        if (token) {
            headers['Authorization'] = `Bearer ${token}`;
            headers['X-Session-Token'] = token;
        }

        if (options.body && typeof options.body === 'object' && !(options.body instanceof FormData)) {
            headers['Content-Type'] = 'application/json';
            options.body = JSON.stringify(options.body);
        }

        const config = {
            ...options,
            headers
        };

        let response;
        try {
            response = await fetch(url, config);
        } catch (netErr) {
            throw {
                code: 'NETWORK_ERROR',
                message: 'Unable to connect to the server. Please check your connection.',
                status: 0
            };
        }

        let data = {};
        const contentType = response.headers.get('content-type');
        if (contentType && contentType.includes('application/json')) {
            try {
                data = await response.json();
            } catch (jsonErr) {
                data = { message: 'Failed to parse JSON response' };
            }
        } else {
            const text = await response.text();
            data = { message: text };
        }

        if (!response.ok) {
            // Handle session expiration
            if (response.status === 401 && endpoint !== '/auth/login') {
                this.clearSession();
                window.dispatchEvent(new CustomEvent('bms:unauthorized'));
            }

            const err = {
                status: response.status,
                code: data.code || `HTTP_${response.status}`,
                message: data.message || 'An unexpected error occurred.',
                field: data.field || null
            };
            throw err;
        }

        return data;
    },

    // Auth endpoints
    async login(userId, password, remember = false) {
        const res = await this.request('/auth/login', {
            method: 'POST',
            body: { user_id: userId, password }
        });
        if (res.token && res.user) {
            this.setToken(res.token, remember);
            this.setUser(res.user, remember);
        }
        return res;
    },

    async logout() {
        try {
            await this.request('/auth/logout', { method: 'POST' });
        } catch (e) {
            console.warn('Logout API call failed or already invalidated', e);
        } finally {
            this.clearSession();
        }
    },

    // Dashboard endpoint (role-specific data)
    async getDashboard() {
        return await this.request('/dashboard', { method: 'GET' });
    },

    // Staff endpoints (Admin only)
    async getStaff() {
        return await this.request('/staff', { method: 'GET' });
    },

    async createOfficer(officerData) {
        return await this.request('/staff', {
            method: 'POST',
            body: officerData
        });
    },

    async updateOfficer(id, data) {
        return await this.request(`/staff/${encodeURIComponent(id)}`, {
            method: 'PATCH',
            body: data
        });
    },

    // User management endpoints (Admin only unlock)
    async getAllUsers() {
        return await this.request('/staff/all-users', { method: 'GET' });
    },

    async unlockUser(id) {
        return await this.request(`/users/${encodeURIComponent(id)}/unlock`, {
            method: 'PATCH'
        });
    }
};

window.api = api;
