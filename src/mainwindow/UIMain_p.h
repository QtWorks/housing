/****************************************************************************
**
**         Created using Monkey Studio IDE v1.9.0.4 (1.9.0.4)
** Authors   : Filipe Azevedo aka Nox P@sNox <pasnox@gmail.com>
** Project   : Housing
** FileName  : UIMain_p.h
** Date      : 2012-12-02T21:57:14
** License   : GPL3
** Home Page : https://github.com/pasnox/housing
** Comment   : An application to search for rent / purchase of appartment / house.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
**
** This package is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program. If not, see <http://www.gnu.org/licenses/>.
**
****************************************************************************/
#ifndef UIMAIN_P_H
#define UIMAIN_P_H

#include <QObject>
#include <QMainWindow>
#include <QEvent>
#include <QNetworkReply>
#include <QPixmapCache>
#if defined(QT_WEBKIT_LIB)
#include <QWebView>
#define WebView QWebView
#elif defined(QT_WEBENGINEWIDGETS_LIB)
#include <QWebEngineView>
#define WebView QWebEngineView
#endif
#include <QSettings>
#include <QMovie>
#include <QDesktopServices>
#include <QDebug>

#if defined( DESKTOP_UI )
    #include "ui_UIDesktopMain.h"
    #include "UIDesktopMain.h"
    typedef Ui_UIDesktopMain Ui_UIMain;
    typedef UIDesktopMain UIMain;
#elif defined( MOBILE_UI )
    #include "ui_UIMobileMain.h"
    #include "UIMobileMain.h"
    typedef Ui_UIMobileMain Ui_UIMain;
    typedef UIMobileMain UIMain;
#else
#error Unknow interface to use.
#endif

#include "interface/AnnouncementModel.h"
#include "interface/AnnouncementProxyModel.h"
#include "objects/NetworkManager.h"
#include "objects/Housing.h"

#define TEST_CASE_DEBUG false

class UIMainPrivate : public QObject
{
    Q_OBJECT

public:
    QMainWindow* widget;
    Ui_UIMain* ui;
    QMovie* movie;
    QLabel* movieLabel;
    AnnouncementModel* model;
    AnnouncementProxyModel* proxy;

public:
    UIMainPrivate( QMainWindow* _widget )
        : QObject( _widget ),
            widget( _widget ),
            ui( new Ui_UIMain ),
            movie( new QMovie( ":/animations/loading.gif" ) ),
            movieLabel( new QLabel ),
            model( new AnnouncementModel( this ) ),
            proxy( new AnnouncementProxyModel( model ) )
    {
        movie->setSpeed( 70 );
        movie->setScaledSize( QSize( 15, 15 ) );
        movie->start();

        movieLabel->setMovie( movie );

        ui->setupUi( widget );
        ui->lvAnnouncements->setModel( proxy );
        ui->tbActions->addSeparator();
        ui->tbActions->addAction( proxy->filterAction() );
#if defined( DESKTOP_UI )
        ui->tbActions->addSeparator();
        ui->tbActions->addAction( ui->dwInputSearch->toggleViewAction() );
        ui->tbActions->addAction( ui->dwFeedback->toggleViewAction() );
        ui->lvAnnouncements->addActions( ui->tbActions->actions() );

        ui->dwInputSearch->toggleViewAction()->setIcon( ui->twPages->tabIcon( 0 ) );
        ui->dwFeedback->toggleViewAction()->setIcon( QIcon::fromTheme( "help-feedback" ) );
        ui->dwFeedback->toggleViewAction()->setChecked( false );
#elif defined( MOBILE_UI )
#endif
        ui->lLegend->setText(
            QString( "<html><head/><body><p><span style=\"text-decoration: line-through; color:#a0a0a0;\">%1</span> | <span style=\"font-weight:600;\">%2</span> | <span style=\"color:#0000ff;\">%3</span></p></body></html>" )
                .arg( UIMain::tr( "Ignored" ) )
                .arg( UIMain::tr( "Bookmarked" ) )
                .arg( UIMain::tr( "Normal" ) )
        );

        connect( NetworkManager::instance(), SIGNAL( finished( QNetworkReply* ) ), this, SLOT( networkRequest_finished( QNetworkReply* ) ) );
        connect( NetworkManager::instance(), SIGNAL( imageFinished( QNetworkReply*, const QByteArray&, const QPixmap& ) ), this, SLOT( networkRequest_imageFinished( QNetworkReply*, const QByteArray&, const QPixmap& ) ) );
        connect( model, SIGNAL( requestImageDownload( const QString& ) ), this, SLOT( model_requestImageDownload( const QString& ) ) );
        connect( model, SIGNAL( requestFetchMore() ), this, SLOT( model_requestFetchMore() ) );
        connect( proxy, SIGNAL( rowCountChanged( int ) ), this, SLOT( proxy_rowCountChanged( int ) ) );
        connect( ui->aStartSearch, SIGNAL( triggered() ), this, SLOT( startSearch_triggered() ) );
#if defined( DESKTOP_UI )
        connect( ui->pbSearch, SIGNAL( clicked() ), ui->aStartSearch, SLOT( trigger() ) );
        connect( ui->twPages, SIGNAL( tabCloseRequested( int ) ), this, SLOT( twPages_tabCloseRequested( int ) ) );
#endif
        connect( ui->lvAnnouncements->selectionModel(), SIGNAL( selectionChanged( const QItemSelection&, const QItemSelection& ) ), this, SLOT( lvAnnouncements_selectionChanged( const QItemSelection&, const QItemSelection& ) ) );
        connect( ui->aSwitchAnnouncementIgnoreState, SIGNAL( triggered( bool ) ), this, SLOT( switchSelectedAnnouncementIgnoreState( bool ) ) );
        connect( ui->aSwitchAnnouncementBookmarkState, SIGNAL( triggered( bool ) ), this, SLOT( switchSelectedAnnouncementBookmarkState( bool ) ) );
#if !defined( QT_NO_WEBKIT )
        connect( ui->aOpenAnnouncementInNewTab, SIGNAL( triggered() ), this, SLOT( openSelectedAnnouncementInNewTab() ) );
        connect( ui->aGeotagAnnouncementInNewTab, SIGNAL( triggered() ), this, SLOT( geotagSelectedAnnouncementInNewTab() ) );
#endif
        connect( ui->aOpenAnnouncementInDefaultBrowser, SIGNAL( triggered() ), this, SLOT( openSelectedAnnouncementInDefaultBrowser() ) );
        connect( ui->aGeotagAnnouncementInDefaultBrowser, SIGNAL( triggered() ), this, SLOT( geotagSelectedAnnouncementInDefaultBrowser() ) );

        lvAnnouncements_selectionChanged();
    }

    ~UIMainPrivate() {
        delete movie;
        delete ui;
    }

    void retranslateUi() {
        ui->retranslateUi( widget );
    }

    void sendSearchRequest( int page ) {
        AbstractHousingDriver* driver = ui->iswSearch->currentDriver();

        if ( !driver ) {
            return;
        }

        model->setIgnoredIdSet( ui->iswSearch->ignoredIdSet() );
        model->setBookmarkedIdSet( ui->iswSearch->bookmarkedIdSet() );

        if ( TEST_CASE_DEBUG ) {
            Announcement::List announcements;
            AbstractHousingDriver::RequestResultProperties properties;
            const bool ok = driver->parseSearchRequestData( driver->testCase(), announcements, &properties );

            model->setCurrentPage( properties.page );
            model->setCanFetchMore( properties.hasNextPage );

            if ( ok ) {
                model->setAnnouncements( announcements );
            }
            else {
                qWarning( "%s: Testcase error", Q_FUNC_INFO );
            }
        }
        else {
            const AbstractHousingDriver::RequestProperties properties = ui->iswSearch->requestProperties();
            QNetworkRequest request;
            QByteArray data;

            driver->setUpSearchRequest( request, data, properties, page );

            if ( data.isEmpty() ) {
                NetworkManager::instance()->get( request );
            }
            else {
                NetworkManager::instance()->post( request, data );
            }

            ui->lvAnnouncements->setCornerWidget( movieLabel );
        }
    }

    QModelIndex selectedSourceIndex() const {
        return proxy->mapToSource( ui->lvAnnouncements->selectionModel()->selectedIndexes().value( 0 ) );
    }

    QString feedbackFileName( const QModelIndex& index ) const {
        return QString( "%1.%2.feedbacks" )
            .arg( ui->iswSearch->currentDriverName() )
            .arg( index.data( AnnouncementModel::IdRole ).toInt() )
        ;
    }

public slots:
    void  networkRequest_finished( QNetworkReply* reply ) {
        AbstractHousingDriver* driver = ui->iswSearch->currentDriver();

        if ( driver && driver->isOwnUrl( reply->request().url() ) ) {
            Announcement::List announcements;
            AbstractHousingDriver::RequestResultProperties properties;
            const bool ok = driver->parseSearchRequestData( reply->readAll(), announcements, &properties );

            if ( ok ) {
                model->setCurrentPage( properties.page );
                model->setCanFetchMore( properties.hasNextPage );
                model->addAnnouncements( announcements );
            }

            ui->lTotalPagesNumber->setNum( properties.totalPage );
            ui->lFoundNumber->setNum( properties.found );
            ui->lVisibleNumber->setNum( properties.visible );
            ui->lAnnouncementsNumber->setNum( proxy->rowCount() );
        }

        ui->lvAnnouncements->setCornerWidget( 0 );
    }

    void networkRequest_imageFinished( QNetworkReply* reply, const QByteArray& data, const QPixmap& pixmap ) {
        Q_UNUSED( data );

        const QString key = reply->request().url().toString();

        if ( !QPixmapCache::insert( key, pixmap ) ) {
            qWarning( "%s: Can not cache pixmap %s", Q_FUNC_INFO, qPrintable( key ) );
        }

        model->update();
    }

    void model_requestImageDownload( const QString& url ) {
        NetworkManager::instance()->getImage( QNetworkRequest( url ) );
    }

    void model_requestFetchMore() {
        sendSearchRequest( model->currentPage() +1 );
    }

    void proxy_rowCountChanged( int count ) {
        ui->lAnnouncementsNumber->setNum( count );
        lvAnnouncements_selectionChanged();
    }

#if !defined( QT_NO_WEBKIT )
    void view_loadFinished( bool ok ) {
        Q_UNUSED( ok );
#if defined( DESKTOP_UI )
        WebView* view = qobject_cast<WebView*>( sender() );
        const int index = ui->twPages->indexOf( view );
        ui->twPages->setTabIcon( index, view->icon() );
        ui->twPages->setTabText( index, view->title() );
        ui->twPages->setTabToolTip( index, view->url().toString() );
#endif
    }
#endif

    void lvAnnouncements_selectionChanged( const QItemSelection& selected = QItemSelection(), const QItemSelection& deselected = QItemSelection() ) {
        Q_UNUSED( selected );

        const QModelIndex index = selectedSourceIndex();
        const QString url = index.data( AnnouncementModel::UrlRole ).toString();
        const int id = index.data( AnnouncementModel::IdRole ).toInt();

        if ( !deselected.isEmpty() ) {
            ui->fwFeedback->saveFileName( feedbackFileName( deselected.indexes().value( 0 ) ) );
        }

        ui->aSwitchAnnouncementIgnoreState->setEnabled( index.isValid() );
        ui->aSwitchAnnouncementIgnoreState->setChecked( index.isValid() && ui->iswSearch->isIgnoredId( id ) );
        ui->aSwitchAnnouncementBookmarkState->setEnabled( index.isValid() );
        ui->aSwitchAnnouncementBookmarkState->setChecked( index.isValid() && ui->iswSearch->isBookmarkedId( id ) );
        ui->aOpenAnnouncementInNewTab->setEnabled( index.isValid() );
        ui->aOpenAnnouncementInDefaultBrowser->setEnabled( index.isValid() );
        ui->aGeotagAnnouncementInNewTab->setEnabled( index.isValid() );
        ui->aGeotagAnnouncementInDefaultBrowser->setEnabled( index.isValid() );

        if ( !index.isValid() ) {
            ui->fwFeedback->clear();
        }
        else {
            ui->fwFeedback->loadFileName( feedbackFileName( index ) );
        }

        ui->fwFeedback->setEnabled( index.isValid() );
    }

    void startSearch_triggered() {
        model->clear();
        sendSearchRequest( 1 );
#if defined( MOBILE_UI )
        ui->swPages->setCurrentWidget( ui->wResults );
#endif
    }

    void twPages_tabCloseRequested( int index ) {
#if defined( DESKTOP_UI )
        QWidget* w = ui->twPages->widget( index );

        if ( w == ui->wSearchResults ) {
            model->clear();
            ui->lTotalPagesNumber->setNum( 0 );
            ui->lFoundNumber->setNum( 0 );
            ui->lVisibleNumber->setNum( 0 );
            ui->lAnnouncementsNumber->setNum( 0 );
            return;
        }

        if ( w ) {
            w->deleteLater();
        }
#endif
    }

    void switchSelectedAnnouncementIgnoreState( bool ignore ) {
        const QModelIndex index = selectedSourceIndex();
        const QString url = index.data( AnnouncementModel::UrlRole ).toString();
        const int id = index.data( AnnouncementModel::IdRole ).toInt();
        ui->iswSearch->setIgnoredId( id, ignore );
        model->setIgnoredIdSet( ui->iswSearch->ignoredIdSet() );

        if ( ignore && ui->iswSearch->isBookmarkedId( id ) ) {
            ui->aSwitchAnnouncementBookmarkState->trigger();
        }

        proxy->invalidate();
    }

    void switchSelectedAnnouncementBookmarkState( bool bookmark ) {
        const QModelIndex index = selectedSourceIndex();
        const QString url = index.data( AnnouncementModel::UrlRole ).toString();
        const int id = index.data( AnnouncementModel::IdRole ).toInt();
        ui->iswSearch->setBookmarkedId( id, bookmark );
        model->setBookmarkedIdSet( ui->iswSearch->bookmarkedIdSet() );

        if ( bookmark && ui->iswSearch->isIgnoredId( id ) ) {
            ui->aSwitchAnnouncementIgnoreState->trigger();
        }

        proxy->invalidate();
    }

#if !defined( QT_NO_WEBKIT )
    void openSelectedAnnouncementInNewTab() {
#if defined( DESKTOP_UI )
        const QModelIndex index = selectedSourceIndex();
        const QString url = index.data( AnnouncementModel::UrlRole ).toString();
        const int id = index.data( AnnouncementModel::IdRole ).toInt();

        foreach ( WebView* view, ui->twPages->findChildren<WebView*>() ) {
            if ( view->url() == url ) {
                return;
            }
        }

        WebView* view = new WebView;
        ui->twPages->addTab( view, UIMain::tr( "Loading %1..." ).arg( id ) );
        connect( view, SIGNAL( loadFinished( bool ) ), this, SLOT( view_loadFinished( bool ) ) );
        view->setUrl( url );
#endif
    }
#endif

    void openSelectedAnnouncementInDefaultBrowser() {
        const QModelIndex index = selectedSourceIndex();
        const QString url = index.data( AnnouncementModel::UrlRole ).toString();
        QDesktopServices::openUrl( url );
    }

#if !defined( QT_NO_WEBKIT )
    void geotagSelectedAnnouncementInNewTab() {
#if defined( DESKTOP_UI )
        const QModelIndex index = selectedSourceIndex();
        const Announcement announcement = index.data( AnnouncementModel::AnnouncementRole ).value<Announcement>();
        const QString url = Housing::googleMapGPSUrl( announcement.latitudeLocation(), announcement.longitudeLocation() );

        foreach ( WebView* view, ui->twPages->findChildren<WebView*>() ) {
            if ( view->url() == url ) {
                return;
            }
        }

        WebView* view = new WebView;
        ui->twPages->addTab( view, UIMain::tr( "Geotaging %1..." ).arg( announcement.id() ) );
        connect( view, SIGNAL( loadFinished( bool ) ), this, SLOT( view_loadFinished( bool ) ) );
        view->setUrl( url );
#endif
    }
#endif

    void geotagSelectedAnnouncementInDefaultBrowser() {
        const QModelIndex index = selectedSourceIndex();
        const Announcement announcement = index.data( AnnouncementModel::AnnouncementRole ).value<Announcement>();
        const QString url = Housing::googleMapGPSUrl( announcement.latitudeLocation(), announcement.longitudeLocation() );
        QDesktopServices::openUrl( url );
    }
};

#endif // UIMAIN_P_H
