import { bootstrapLinkApi, hasInitialImportRequest, hasInitialInstallRequest } from '@pwa/service/link-api.service';

import { AppModule } from './app/app.module';
import { enableProdMode } from '@angular/core';
import { environment } from './environments/environment';
import { hasStoredSession } from '@pwa/helper/storedSession';
import { platformBrowserDynamic } from '@angular/platform-browser-dynamic';

// Work around issue with webpack and deep-equal
(window as any).global = window;

bootstrapLinkApi();

if ((hasStoredSession() && !hasInitialImportRequest()) || hasInitialInstallRequest()) {
    location.replace(`${location.origin}${location.pathname}#/tab/emulation`);
} else {
    location.replace(`${location.origin}${location.pathname}#/tab/sessions`);
}

if (environment.production) {
    enableProdMode();
}

// work around status bar overlapping the viewport after rotation on iOS

window.addEventListener('orientationchange', () =>
    setTimeout(() => {
        document.body.style.height = window.innerHeight + 'px';
    }, 100)
);

window.addEventListener('scroll', () => window.scrollTo(0, 0));
document.body.addEventListener('contextmenu', (e) => e.preventDefault());

platformBrowserDynamic()
    .bootstrapModule(AppModule)
    .catch((err) => console.log(err));
