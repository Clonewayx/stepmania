// SMPackageInstallDlg.cpp : implementation file
//

#define CO_EXIST_WITH_MFC
#include "global.h"
#include "stdafx.h"
#include "smpackage.h"
#include "SMPackageInstallDlg.h"
#include "RageUtil.h"
#include "smpackageUtil.h"
#include "EditInsallations.h"
#include "ShowComment.h"
#include "IniFile.h"	
#include "UninstallOld.h"	
#include <algorithm>	
#include "RageFileManager.h"	
#include "RageFileDriverZip.h"	
#include "archutils/Win32/DialogUtil.h"
#include "LocalizedString.h"
#include "RageLog.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSMPackageInstallDlg dialog


CSMPackageInstallDlg::CSMPackageInstallDlg(RString sPackagePath, CWnd* pParent /*=NULL*/)
	: CDialog(CSMPackageInstallDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CSMPackageInstallDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	m_sPackagePath = sPackagePath;
}


void CSMPackageInstallDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSMPackageInstallDlg)
	DDX_Control(pDX, IDC_BUTTON_EDIT, m_buttonEdit);
	DDX_Control(pDX, IDC_COMBO_DIR, m_comboDir);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSMPackageInstallDlg, CDialog)
	//{{AFX_MSG_MAP(CSMPackageInstallDlg)
	ON_WM_PAINT()
	ON_BN_CLICKED(IDC_BUTTON_EDIT, OnButtonEdit)
	//}}AFX_MSG_MAP
	ON_WM_CTLCOLOR()
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CSMPackageInstallDlg message handlers

static bool CompareStringNoCase( const RString &s1, const RString &s2 )
{
	return s1.CompareNoCase( s2 ) < 0;
}

static LocalizedString COULD_NOT_OPEN_FILE		( "CSMPackageInstallDlg", "Could not open file '%s'." );
static LocalizedString IS_NOT_A_VALID_ZIP		( "CSMPackageInstallDlg", "'%s' is not a valid zip archive." );
static LocalizedString YOU_HAVE_CHOSEN_TO_INSTALL	( "CSMPackageInstallDlg", "You have chosen to install the package:" );
static LocalizedString THIS_PACKAGE_CONTAINS		( "CSMPackageInstallDlg", "This package contains the following files:" );
static LocalizedString THE_PACKAGE_WILL_BE_INSTALLED	( "CSMPackageInstallDlg", "The package will be installed in the following program folder:" );
BOOL CSMPackageInstallDlg::OnInitDialog() 
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	DialogUtil::LocalizeDialogAndContents( *this );
	DialogUtil::SetHeaderFont( *this, IDC_STATIC_HEADER_TEXT );

	// mount the zip
	RageFileOsAbsolute file;
	if( !file.Open(m_sPackagePath) )
	{
		MessageBox( ssprintf(COULD_NOT_OPEN_FILE.GetValue(),m_sPackagePath.c_str()) );
		exit(1);	// better way to abort?
	}

	RageFileDriverZip zip;
	if( !zip.Load(&file) )
	{
		MessageBox( ssprintf(IS_NOT_A_VALID_ZIP.GetValue(),m_sPackagePath.c_str()) );
		exit(1);	// better way to abort?
	}

	//
	// Set the text of the first Edit box
	//
	RString sMessage1 = ssprintf(
		YOU_HAVE_CHOSEN_TO_INSTALL.GetValue()+"\r\n"
		"\r\n"
		"\t%s\r\n"
		"\r\n" +
		THIS_PACKAGE_CONTAINS.GetValue()+"\r\n",
		m_sPackagePath.c_str()
	);
	CEdit* pEdit1 = (CEdit*)GetDlgItem(IDC_EDIT_MESSAGE1);
	pEdit1->SetWindowText( sMessage1 );


	//
	// Set the text of the second Edit box
	//
	{
		vector<RString> vs;
		GetDirListingRecursive( &zip, "/", "*", vs );
		CEdit* pEdit2 = (CEdit*)GetDlgItem(IDC_EDIT_MESSAGE2);
		RString sText = "\t" + join( "\r\n\t", vs );
		pEdit2->SetWindowText( sText );
	}


	//
	// Set the text of the third Edit box
	//
	RString sMessage3 = THE_PACKAGE_WILL_BE_INSTALLED.GetValue()+"\r\n";

	// Set the message
	CEdit* pEdit3 = (CEdit*)GetDlgItem(IDC_EDIT_MESSAGE3);
	pEdit3->SetWindowText( sMessage3 );


	RefreshInstallationList();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


void CSMPackageInstallDlg::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	
	// TODO: Add your message handler code here
	
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}
#include <direct.h>
#include ".\smpackageinstalldlg.h"

static bool CheckPackages( RageFileDriverZip &fileDriver )
{
	int iErr;
	RageFileBasic *pFile = fileDriver.Open( "smzip.ctl", RageFile::READ, iErr );
	if( pFile == NULL )
		return true;

	IniFile ini;
	ini.ReadFile( *pFile );

	int version = 0;
	ini.GetValue( "SMZIP", "Version", version );
	if( version != 1 )
		return true;

	int cnt = 0;
	ini.GetValue( "Packages", "NumPackages", cnt );

	vector<RString> Directories;
	for( int i = 0; i < cnt; ++i )
	{
		RString path;
		if( !ini.GetValue( "Packages", ssprintf("%i", i), path) )
			continue;

		/* Does this directory exist? */
		if( !FILEMAN->IsADirectory(path) )
			continue;

		if( !SMPackageUtil::IsValidPackageDirectory(path) )
			continue;
		
		Directories.push_back(path);
	}

	if( Directories.empty() )
		return true;

	{
		UninstallOld UninstallOldDlg;
		UninstallOldDlg.m_sPackages = join("\r\n", Directories);
		int nResponse = UninstallOldDlg.DoModal();
		if( nResponse == IDCANCEL )
			return false;	// cancelled
		if( nResponse == IDIGNORE )
			return true;
	}

	char cwd_[MAX_PATH];
	_getcwd(cwd_, MAX_PATH);
	RString cwd(cwd_);
	if( cwd[cwd.size()-1] != '\\' )
		cwd += "\\";

	for( i = 0; i < (int) Directories.size(); ++i )
	{
		RString sDir = Directories[i];
		if( !DeleteRecursive(sDir) )	// error deleting
		{
			return false;
		}
	}

	return true;
}

static LocalizedString NO_INSTALLATIONS			("CSMPackageInstallDlg", "No Installations found.  Exiting.");
static LocalizedString INSTALLING_PLEASE_WAIT		("CSMPackageInstallDlg", "Installing '%s'.  Please wait...");
static LocalizedString ERROR_OPENING_SOURCE_FILE	("CSMPackageInstallDlg", "Error opening source file '%s': %s");
static LocalizedString ERROR_OPENING_DESTINATION_FILE	("CSMPackageInstallDlg", "Error opening destination file '%s': %s");
static LocalizedString ERROR_COPYING_FILE		("CSMPackageInstallDlg", "Error copying file '%s': %s");
static LocalizedString PACKAGE_INSTALLED_SUCCESSFULLY	("CSMPackageInstallDlg","Package installed successfully!");
void CSMPackageInstallDlg::OnOK() 
{
	// TODO: Add extra validation here

	int ProgressInit = 0;

	m_comboDir.EnableWindow( FALSE );
	m_buttonEdit.EnableWindow( FALSE );

	RString sInstallDir;
	{
		CString s;
		m_comboDir.GetWindowText( s );
		sInstallDir = s;
	}

	// selected install dir becomes the new default
	int iSelectedInstallDirIndex = m_comboDir.GetCurSel();
	if( iSelectedInstallDirIndex == -1 )
	{
		MessageBox( NO_INSTALLATIONS.GetValue(), NULL, MB_OK|MB_ICONEXCLAMATION );
		return;
	}

	SMPackageUtil::SetDefaultInstallDir( iSelectedInstallDirIndex );


	// mount the zip
	RageFileOsAbsolute file;
	if( !file.Open(m_sPackagePath) )
	{
		MessageBox( ssprintf(COULD_NOT_OPEN_FILE.GetValue(),m_sPackagePath.c_str()) );
		exit(1);	// better way to abort?
	}

	RageFileDriverZip zip;
	if( !zip.Load(&file) )
	{
		MessageBox( ssprintf(IS_NOT_A_VALID_ZIP.GetValue(),m_sPackagePath.c_str()) );
		exit(1);	// better way to abort?
	}

	// Show comment (if any)
	{
		RString sComment = zip.GetGlobalComment();
		bool DontShowComment;
		if( sComment != "" && (!SMPackageUtil::GetPref("DontShowComment", DontShowComment) || !DontShowComment) )
		{
			ShowComment commentDlg;
			commentDlg.m_sComment = sComment;
			int nResponse = commentDlg.DoModal();
			if( nResponse != IDOK )
				return;	// cancelled
			if( commentDlg.m_bDontShow )
				SMPackageUtil::SetPref( "DontShowComment", true );
		}
	}

	/* Check for installed packages that should be deleted before installing. */
	if( !CheckPackages(zip) )
		return;	// cancelled


	// Unzip the SMzip package into the installation folder
	vector<RString> vs;
	GetDirListingRecursive( &zip, "/", "*", vs );
	for( unsigned i=0; i<vs.size(); i++ )
	{
		// Throw some text up so the user has something to look at during the long pause.
		CEdit* pEdit1 = (CEdit*)GetDlgItem(IDC_EDIT_MESSAGE1);
		pEdit1->SetWindowText( ssprintf(INSTALLING_PLEASE_WAIT.GetValue(), m_sPackagePath.c_str()) );
		CEdit* pEdit2 = (CEdit*)GetDlgItem(IDC_EDIT_MESSAGE2);
		pEdit2->SetWindowText( "" );
		CEdit* pEdit3 = (CEdit*)GetDlgItem(IDC_EDIT_MESSAGE3);
		pEdit3->SetWindowText( "" );
		CProgressCtrl* pProgress1 = (CProgressCtrl*)GetDlgItem(IDC_PROGRESS1);
		//Show the hided progress bar
		if(!pProgress1->IsWindowVisible())
		{
			pProgress1->ShowWindow(SW_SHOWNORMAL);
		}
		//Initialize the progress bar and update the window 1 time (it's enough)
		if(!ProgressInit)
		{
			pProgress1->SetRange( 0, (short)vs.size() );
			pProgress1->SetStep(1);
			pProgress1->SetPos(0);
			SendMessage( WM_PAINT );
			UpdateWindow();		// Force the silly thing to hadle WM_PAINT now!
			ProgressInit = 1;
		}

retry_unzip:

		// Extract the files
		const RString sFile = vs[i];
		LOG->Trace( "Extracting: "+sFile );

		// skip extracting "thumbs.db" files
		if( Basename(sFile).CompareNoCase("thumbs.db") == 0 )
			continue;

		RString sError;
		{
			int iErr;
			RageFileBasic *pFileFrom = zip.Open( sFile, RageFile::READ, iErr );
			if( pFileFrom == NULL )
			{
				sError = ssprintf( ERROR_OPENING_SOURCE_FILE.GetValue(), sFile.c_str(), ssprintf("%d",iErr).c_str() );
				goto show_error;
			}

			RageFile fileTo;		
			if( !fileTo.Open(sFile, RageFile::WRITE) )
			{
				sError = ssprintf( ERROR_OPENING_DESTINATION_FILE.GetValue(), sFile.c_str(), fileTo.GetError().c_str() );
				goto show_error;
			}

			RString sErr;
			if( !FileCopy(*pFileFrom, fileTo, sErr) )
			{
				sError = ssprintf( ERROR_COPYING_FILE.GetValue(), sFile.c_str(), sErr.c_str() );
				goto show_error;
			}
		}

		goto done_with_file;

show_error:
		switch( MessageBox( sError, NULL, MB_ABORTRETRYIGNORE|MB_ICONEXCLAMATION ) )
		{
		case IDABORT:
			exit(1);	// better way to exit?
			break;
		case IDRETRY:
			goto retry_unzip;
			break;
		case IDIGNORE:
			// do nothing
			break;
		}

done_with_file:

		pProgress1->StepIt(); //increase the progress bar of 1 step
	}

	AfxMessageBox( PACKAGE_INSTALLED_SUCCESSFULLY.GetValue() );

	// close the dialog
	CDialog::OnOK();
}


void CSMPackageInstallDlg::OnButtonEdit() 
{
	// TODO: Add your control notification handler code here
	EditInsallations dlg;
	int nResponse = dlg.DoModal();
	if( nResponse == IDOK )
	{
		RefreshInstallationList();
	}
}


void CSMPackageInstallDlg::RefreshInstallationList() 
{
	m_comboDir.ResetContent();

	vector<RString> asInstallDirs;
	SMPackageUtil::GetGameInstallDirs( asInstallDirs );
	for( unsigned i=0; i<asInstallDirs.size(); i++ )
		m_comboDir.AddString( asInstallDirs[i] );
	m_comboDir.SetCurSel( 0 );	// guaranteed to be at least one item
}

HBRUSH CSMPackageInstallDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
	HBRUSH hbr = CDialog::OnCtlColor(pDC, pWnd, nCtlColor);

	// TODO:  Change any attributes of the DC here
	if( pWnd->GetDlgCtrlID() == IDC_STATIC_HEADER_TEXT )
	{
        hbr = (HBRUSH)::GetStockObject(HOLLOW_BRUSH); 
        pDC->SetBkMode(TRANSPARENT); 
	}

	// TODO:  Return a different brush if the default is not desired
	return hbr;
}

/*
 * (c) 2002-2005 Chris Danford
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */