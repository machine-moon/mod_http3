document.addEventListener('DOMContentLoaded', () => {
    // Handling dropdown toggles on mobile or click
    const dropdownToggles = document.querySelectorAll('.dropdown-toggle');
    
    dropdownToggles.forEach(toggle => {
        toggle.addEventListener('click', (e) => {
            e.preventDefault();
            const parent = toggle.closest('.nav-dropdown');
            const menu = parent.querySelector('.dropdown-menu');
            
            // Toggle active state
            const isActive = parent.classList.contains('active');
            
            // Close all other dropdowns
            document.querySelectorAll('.nav-dropdown').forEach(dropdown => {
                dropdown.classList.remove('active');
            });

            if (!isActive) {
                parent.classList.add('active');
            }
        });
    });

    // Close dropdowns when clicking outside
    document.addEventListener('click', (e) => {
        if (!e.target.closest('.nav-dropdown')) {
            document.querySelectorAll('.nav-dropdown').forEach(dropdown => {
                dropdown.classList.remove('active');
            });
        }
    });
});
